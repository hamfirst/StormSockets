
#include "StormSocketClientFrontendHttp.h"
#include "StormSocketConnectionHttp.h"

namespace StormSockets
{
  StormSocketClientFrontendHttp::StormSocketClientFrontendHttp(const StormSocketClientFrontendHttpSettings & settings, StormSocketBackend * backend) :
    StormSocketFrontendHttpBase(settings, backend),
    m_ConnectionAllocator(sizeof(StormSocketClientConnectionHttp) * settings.MaxConnections, sizeof(StormSocketClientConnectionHttp), false)
  {
    InitClientSSL(m_SSLData);
  }

  StormSocketClientFrontendHttp::~StormSocketClientFrontendHttp()
  {
    CleanupAllConnections();
    ReleaseClientSSL(m_SSLData);
  }

  StormSocketConnectionId StormSocketClientFrontendHttp::RequestConnect(const char * ip_addr, int port, const StormSocketClientFrontendHttpRequestData & request_data)
  {
    return m_Backend->RequestConnect(this, ip_addr, port, &request_data);
  }

  StormSocketConnectionId StormSocketClientFrontendHttp::RequestConnect(const StormURI & uri)
  {
    auto request = m_Backend->CreateHttpRequestWriter("GET", uri.m_Uri.c_str(), uri.m_Host.c_str());

    auto port = atoi(uri.m_Port.c_str());
    auto connection_id = m_Backend->RequestConnect(this, uri.m_Host.c_str(), port, &request);
    m_Backend->FreeOutgoingHttpRequest(request);

    return connection_id;
  }

  StormSocketConnectionId StormSocketClientFrontendHttp::RequestConnect(const char * url)
  {
    StormURI uri;
    if (ParseURI(url, uri) == false)
    {
      return StormSocketConnectionId::InvalidConnectionId;
    }

    return RequestConnect(uri);
  }

  void StormSocketClientFrontendHttp::FreeIncomingHttpResponse(StormHttpResponseReader & reader)
  {
    m_Backend->DiscardReaderData(reader.m_ConnectionId, reader.m_FullDataLen);
  }

#ifdef USE_MBED
  bool StormSocketClientFrontendHttp::UseSSL(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id)
  {
    auto & http_connection = GetHttpConnection(frontend_id);
    return http_connection.m_UseSSL;
  }
#endif

  StormSocketClientConnectionHttp & StormSocketClientFrontendHttp::GetHttpConnection(StormSocketFrontendConnectionId id)
  {
    StormSocketClientConnectionHttp * ptr = (StormSocketClientConnectionHttp *)m_ConnectionAllocator.ResolveHandle(id);
    return *ptr;
  }

  StormSocketFrontendConnectionId StormSocketClientFrontendHttp::AllocateFrontendId()
  {
    StormFixedBlockHandle handle = m_ConnectionAllocator.AllocateBlock(StormFixedBlockType::Custom);
    if (handle == InvalidBlockHandle)
    {
      return InvalidFrontendId;
    }

    StormSocketClientConnectionHttp * ptr = (StormSocketClientConnectionHttp *)m_ConnectionAllocator.ResolveHandle(handle);
    new (ptr) StormSocketClientConnectionHttp();

    return handle;
  }

  void StormSocketClientFrontendHttp::FreeFrontendId(StormSocketFrontendConnectionId frontend_id)
  {
    auto & http_connection = GetHttpConnection(frontend_id);

    http_connection.~StormSocketClientConnectionHttp();
    m_ConnectionAllocator.FreeBlock(&http_connection, StormFixedBlockType::Custom);
  }

  void StormSocketClientFrontendHttp::InitConnection(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id, const void * init_data)
  {
    StormSocketClientFrontendHttpRequestData * request_data = (StormSocketClientFrontendHttpRequestData *)init_data;

    auto & http_connection = GetHttpConnection(frontend_id);
    http_connection.m_RequestWriter = request_data->m_RequestWriter;
    http_connection.m_UseSSL = request_data->m_UseSSL;

    m_Backend->ReferenceOutgoingHttpRequest(*http_connection.m_RequestWriter);
  }

  void StormSocketClientFrontendHttp::CleanupConnection(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id)
  {
    auto & connection = GetConnection(connection_id);
    auto & http_connection = GetHttpConnection(frontend_id);

    if (http_connection.m_BodyReader)
    {
      if (http_connection.m_CompleteResponse == false && http_connection.m_BodyLength < 0)
      {
        void * parse_block = m_Allocator.ResolveHandle(connection.m_ParseBlock);
        http_connection.m_BodyReader =
          StormHttpResponseReader(parse_block, connection.m_UnparsedDataLength, connection.m_ParseOffset, connection_id, &m_Allocator, &m_MessageReaders,
            *http_connection.m_StatusLine, *http_connection.m_ResponsePhrase, *http_connection.m_Headers);

        CompleteBody(connection_id, http_connection);
      }
    }

    if (http_connection.m_RequestWriter)
    {
      m_Backend->FreeOutgoingHttpRequest(*http_connection.m_RequestWriter);
      http_connection.m_RequestWriter = {};
    }
  }

  bool StormSocketClientFrontendHttp::ProcessData(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id)
  {
    ProfileScope prof(ProfilerCategory::kProcHttp);
    auto & connection = GetConnection(connection_id);
    auto & http_connection = GetHttpConnection(frontend_id);

    while (true)
    {
      if ((connection.m_DisconnectFlags & (int)StormSocketDisconnectFlags::kRemoteClose) != 0 ||
        (connection.m_DisconnectFlags & (int)StormSocketDisconnectFlags::kSignalClose) != 0 ||
        connection.m_FailedConnection)
      {
        m_Backend->DiscardParserData(connection_id, connection.m_UnparsedDataLength);
        return true;
      }

      if (connection.m_ParseBlock == InvalidBlockHandle)
      {
        connection.m_ParseBlock = connection.m_RecvBuffer.m_BlockStart;
      }

      if (http_connection.m_State == StormSocketClientConnectionHttpState::ReadingHeaders)
      {
        if (connection.m_UnparsedDataLength == 0)
        {
          return true;
        }

        StormMessageHeaderReader header_reader(&m_Allocator, m_Allocator.ResolveHandle(connection.m_ParseBlock), connection.m_UnparsedDataLength, connection.m_ParseOffset);

        int full_data_len = 0;
        bool got_header = false;
        StormMessageReaderCursor cur_header = header_reader.AdvanceToNextHeader(full_data_len, got_header);

        ProfileScope prof(ProfilerCategory::kProcHeaders);
        while (got_header)
        {
          bool got_terminator = false;

          if (http_connection.m_GotStatusLine == false)
          {
            // Read the status line
            http_connection.m_StatusLine = cur_header;

            if (ParseStatusLine(cur_header, http_connection) == false)
            {
              ForceDisconnect(connection_id);
              return true;
            }

            http_connection.m_Headers = header_reader;
          }
          else if (cur_header.GetRemainingLength() == 0)
          {
            got_terminator = true;
          }
          else
          {
            int header_val;
            int header_val_lowercase;

            m_HeaderValues.ReadInitialVal(cur_header, header_val, header_val_lowercase);
            if (m_HeaderValues.MatchExact(cur_header, header_val_lowercase, StormHttpHeaderType::ChunkedEncoding))
            {
              http_connection.m_Chunked = true;
            }
            else if (m_HeaderValues.MatchExact(cur_header, header_val_lowercase, StormHttpHeaderType::ContentLength))
            {
              if (cur_header.ReadNumber(http_connection.m_BodyLength) == false)
              {
                ForceDisconnect(connection_id);
                return true;
              }
            }
          }

          DiscardParsedData(connection_id, http_connection, full_data_len);

          if (got_terminator)
          {
            if (http_connection.m_Chunked)
            {
              http_connection.m_State = StormSocketClientConnectionHttpState::ReadingChunkSize;
            }
            else
            {
              if (http_connection.m_BodyLength == 0)
              {
                if (CompleteBody(connection_id, http_connection) == false)
                {
                  return false;
                }
              }
              else
              {
                http_connection.m_State = StormSocketClientConnectionHttpState::ReadingBody;
              }
            }
            break;
          }

          cur_header = header_reader.AdvanceToNextHeader(full_data_len, got_header);
        }

        if (http_connection.m_State == StormSocketClientConnectionHttpState::ReadingHeaders)
        {
          return true;
        }
      }

      return ProcessHttpData(connection, http_connection, connection_id);
    }
  }

  bool StormSocketClientFrontendHttp::ParseStatusLine(StormMessageReaderCursor & status_line, StormSocketClientConnectionHttp & http_connection)
  {
    int header_val;
    int header_val_lowercase;

    m_HeaderValues.ReadInitialVal(status_line, header_val, header_val_lowercase);

    if (m_HeaderValues.Match(status_line, header_val, StormHttpHeaderType::HttpVer) == false)
    {
      return false;
    }

    if (status_line.ReadByte() != ' ')
    {
      return false;
    }

    if (status_line.ReadNumber(http_connection.m_ResponseCode, 3) == false)
    {
      return false;
    }

    if (status_line.ReadByte() != ' ')
    {
      return false;
    }

    http_connection.m_ResponsePhrase = status_line;
    http_connection.m_GotStatusLine = true;
    return true;
  }

  void StormSocketClientFrontendHttp::ConnectionEstablishComplete(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id)
  {
    auto & http_connection = GetHttpConnection(frontend_id);
    m_Backend->SendHttpRequestToConnection(*http_connection.m_RequestWriter, connection_id);

    m_Backend->FreeOutgoingHttpRequest(*http_connection.m_RequestWriter);
    http_connection.m_RequestWriter = {};
  }

  void StormSocketClientFrontendHttp::AddBodyBlock(StormSocketConnectionId connection_id, StormHttpConnectionBase & http_connection_base, void * chunk_ptr, int chunk_len, int read_offset)
  {
    StormSocketClientConnectionHttp & http_connection = (StormSocketClientConnectionHttp &)http_connection_base;
    if (http_connection.m_BodyReader)
    {
      http_connection.m_BodyReader->AddBlock(chunk_ptr, chunk_len, read_offset);
    }
    else
    {
      http_connection.m_BodyReader =
        StormHttpResponseReader(chunk_ptr, chunk_len, read_offset, connection_id, &m_Allocator, &m_MessageReaders,
          *http_connection.m_StatusLine, *http_connection.m_ResponsePhrase, *http_connection.m_Headers);
    }
  }

  bool StormSocketClientFrontendHttp::CompleteBody(StormSocketConnectionId connection_id, StormHttpConnectionBase & http_connection_base)
  {
    StormSocketClientConnectionHttp & http_connection = (StormSocketClientConnectionHttp &)http_connection_base;
    auto & connection = GetConnection(connection_id);

    if (http_connection.m_BodyReader)
    {
      http_connection.m_BodyReader->FinalizeFullDataLength(http_connection.m_TotalLength);
    }
    else
    {
      http_connection.m_BodyReader =
        StormHttpResponseReader(nullptr, 0, 0, connection_id, &m_Allocator, &m_MessageReaders, 
          *http_connection.m_StatusLine, *http_connection.m_ResponsePhrase, *http_connection.m_Headers);
    }

    StormSocketEventInfo data_message;
    data_message.ConnectionId = connection_id;
    data_message.GetHttpResponseReader() = *http_connection.m_BodyReader;
    data_message.Type = StormSocketEventType::Data;
    data_message.RemoteIP = connection.m_RemoteIP;
    data_message.RemotePort = connection.m_RemotePort;

    if (m_EventQueue.Enqueue(data_message) == false)
    {
      return false;
    }

    m_EventCondition.notify_one();

    http_connection.m_CompleteResponse = true;
    ForceDisconnect(connection_id);
    return true;
  }


  void StormSocketClientFrontendHttp::SendClosePacket(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id)
  {

  }
}
