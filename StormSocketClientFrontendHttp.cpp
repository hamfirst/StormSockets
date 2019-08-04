
#include "StormSocketClientFrontendHttp.h"
#include "StormSocketConnectionHttp.h"
#include "StormSocketLog.h"

namespace StormSockets
{
  StormSocketClientFrontendHttp::StormSocketClientFrontendHttp(const StormSocketClientFrontendHttpSettings & settings, StormSocketBackend * backend) :
    StormSocketFrontendHttpBase(settings, backend),
    m_ConnectionAllocator(sizeof(StormSocketClientConnectionHttp) * settings.MaxConnections, sizeof(StormSocketClientConnectionHttp), false),
    m_SSLData(std::make_unique<StormSocketClientSSLData[]>(kDefaultSSLConfigs))
  {
    for (int index = 0; index < kDefaultSSLConfigs; ++index)
    {
      InitClientSSL(m_SSLData[index], backend);
    }
  }

  StormSocketClientFrontendHttp::~StormSocketClientFrontendHttp()
  {
    CleanupAllConnections();
    for (int index = 0; index < kDefaultSSLConfigs; ++index)
    {
      ReleaseClientSSL(m_SSLData[index]);
    }
  }

  StormSocketConnectionId StormSocketClientFrontendHttp::RequestConnect(const char * ip_addr, int port, const StormSocketClientFrontendHttpRequestData & request_data)
  {
    return m_Backend->RequestConnect(this, ip_addr, port, &request_data);
  }

  StormSocketConnectionId StormSocketClientFrontendHttp::RequestConnect(const StormURI & uri, const void * body, int body_len, const void * headers, int header_len)
  {
    auto writer = m_Backend->CreateHttpRequestWriter(body_len != 0 ? "POST" : "GET", uri.m_Uri.c_str(), 
      uri.m_Port.size() == 0 ? uri.m_Host.c_str() : (uri.m_Host + ":" + uri.m_Port).data());
    StormSocketClientFrontendHttpRequestData request_data{ writer, (uri.m_Protocol == "https") };
    request_data.m_RequestWriter.WriteHeaders(headers, header_len);
    request_data.m_RequestWriter.WriteBody(body, body_len);
    request_data.m_RequestWriter.FinalizeHeaders();

    auto port = atoi(uri.m_Port.c_str());
    if (port == 0)
    {
      if (uri.m_Protocol == "http")
      {
        port = 80;
      }
      else if (uri.m_Protocol == "https")
      {
        port = 443;
      }
    }

    auto connection_id = m_Backend->RequestConnect(this, uri.m_Host.c_str(), port, &request_data);
    m_Backend->FreeOutgoingHttpRequest(request_data.m_RequestWriter);

    return connection_id;
  }

  StormSocketConnectionId StormSocketClientFrontendHttp::RequestConnect(const StormURI & uri, const char * method, const void * body, int body_len, const void * headers, int header_len)
  {
    auto writer = m_Backend->CreateHttpRequestWriter(method, uri.m_Uri.c_str(), uri.m_Host.c_str());
    StormSocketClientFrontendHttpRequestData request_data{ writer, (uri.m_Protocol == "https") };
    request_data.m_RequestWriter.WriteHeaders(headers, header_len);
    request_data.m_RequestWriter.WriteBody(body, body_len);
    request_data.m_RequestWriter.FinalizeHeaders();

    auto port = atoi(uri.m_Port.c_str());
    if (port == 0)
    {
      if (uri.m_Protocol == "http")
      {
        port = 80;
      }
      else if (uri.m_Protocol == "https")
      {
        port = 443;
      }
    }

    auto connection_id = m_Backend->RequestConnect(this, uri.m_Host.c_str(), port, &request_data);
    m_Backend->FreeOutgoingHttpRequest(request_data.m_RequestWriter);

    return connection_id;
  }

  StormSocketConnectionId StormSocketClientFrontendHttp::RequestConnect(const char * url, const void * body, int body_len, const void * headers, int header_len)
  {
    StormURI uri;
    if (ParseURI(url, uri) == false)
    {
      return StormSocketConnectionId::InvalidConnectionId;
    }

    return RequestConnect(uri, body, body_len, headers, header_len);
  }

  void StormSocketClientFrontendHttp::FreeIncomingHttpResponse(StormHttpResponseReader & reader)
  {
    reader.FreeChain();
    m_Backend->DiscardReaderData(reader.m_ConnectionId, reader.m_FullDataLen);
  }
  
  void StormSocketClientFrontendHttp::MemoryAudit()
  {
    StormSocketFrontendBase::MemoryAudit();
    printf("Connection allocator: %d\n", m_ConnectionAllocator.GetOutstandingMallocs());
  }

#ifndef DISABLE_MBED
  bool StormSocketClientFrontendHttp::UseSSL([[maybe_unused]] StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id)
  {
    auto & http_connection = GetHttpConnection(frontend_id);
    return http_connection.m_UseSSL;
  }

  mbedtls_ssl_config * StormSocketClientFrontendHttp::GetSSLConfig(StormSocketFrontendConnectionId frontend_id)
  {
    std::size_t slot = frontend_id.m_Index >= 0 ? frontend_id.m_Index : reinterpret_cast<std::size_t>(frontend_id.m_MallocBlock);
    return &m_SSLData[slot % kDefaultSSLConfigs].m_SSLConfig;
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

  void StormSocketClientFrontendHttp::InitConnection([[maybe_unused]] StormSocketConnectionId connection_id, 
    StormSocketFrontendConnectionId frontend_id, const void * init_data)
  {
    StormSocketClientFrontendHttpRequestData * request_data = (StormSocketClientFrontendHttpRequestData *)init_data;

    auto & http_connection = GetHttpConnection(frontend_id);
    http_connection.m_RequestWriter = request_data->m_RequestWriter;
    http_connection.m_UseSSL = request_data->m_UseSSL;

    m_Backend->ReferenceOutgoingHttpRequest(*http_connection.m_RequestWriter);
  }

  void StormSocketClientFrontendHttp::CleanupConnection([[maybe_unused]] StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id)
  {
    auto & http_connection = GetHttpConnection(frontend_id);

    if (http_connection.m_RequestWriter)
    {
      m_Backend->FreeOutgoingHttpRequest(*http_connection.m_RequestWriter);
      http_connection.m_RequestWriter = {};
    }
  }

  void StormSocketClientFrontendHttp::QueueDisconnectEvent(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id)
  {
    auto & connection = GetConnection(connection_id);
    auto & http_connection = GetHttpConnection(frontend_id);

    if (http_connection.m_CompleteResponse == false && http_connection.m_BodyLength < 0 && http_connection.m_State == StormSocketClientConnectionHttpState::ReadingBody)
    {
      void * parse_block = m_Allocator.ResolveHandle(connection.m_ParseBlock);
      http_connection.m_BodyReader =
        StormHttpResponseReader(parse_block, connection.m_UnparsedDataLength, connection.m_ParseOffset, connection_id, &m_Allocator, &m_MessageReaders,
          *http_connection.m_StatusLine, *http_connection.m_ResponsePhrase, *http_connection.m_Headers, http_connection.m_ResponseCode);

      CompleteBody(connection_id, http_connection);
    }

    if (http_connection.m_CompleteResponse == false) 
    {
      StormSocketLog("Disconnected after getting %d bytes\n", http_connection.m_TotalLength);
    }

    StormSocketFrontendHttpBase::QueueDisconnectEvent(connection_id, frontend_id);
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
              StormSocketLog("Got invalid status line\n");
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
            else if (m_HeaderValues.Match(cur_header, header_val_lowercase, StormHttpHeaderType::ContentLength))
            {
              if (cur_header.ReadNumber(http_connection.m_BodyLength) == false)
              {
                StormSocketLog("Got invalid content length\n");
                ForceDisconnect(connection_id);
                return true;
              }
            }
          }

          DiscardParsedData(connection_id, http_connection, full_data_len);

          if (got_terminator)
          {
            m_Backend->SetHandshakeComplete(connection_id);
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
                if (http_connection.m_BodyLength < 0)
                {
                  if ((http_connection.m_ResponseCode >= 100 && http_connection.m_ResponseCode <= 199) || http_connection.m_ResponseCode == 204 || http_connection.m_ResponseCode == 304)
                  {
                    if (CompleteBody(connection_id, http_connection) == false)
                    {
                      return false;
                    }
                  }
                }

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
      if (m_HeaderValues.Match(status_line, header_val, StormHttpHeaderType::HttpVer1) == false)
      {
        return false;
      }
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
    StormSocketLog("Connection established\n");
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
          *http_connection.m_StatusLine, *http_connection.m_ResponsePhrase, *http_connection.m_Headers, http_connection.m_ResponseCode);
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
          *http_connection.m_StatusLine, *http_connection.m_ResponsePhrase, *http_connection.m_Headers,
          http_connection.m_ResponseCode);
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

    if (m_EventSemaphore)
    {
      m_EventSemaphore->Release();
    }

    http_connection.m_CompleteResponse = true;
    ForceDisconnect(connection_id);
    return true;
  }


  void StormSocketClientFrontendHttp::SendClosePacket([[maybe_unused]] StormSocketConnectionId connection_id, 
    [[maybe_unused]] StormSocketFrontendConnectionId frontend_id)
  {

  }
}
