
#include "StormSocketServerFrontendHttp.h"

#include <hash/Hash.h>

namespace StormSockets
{
  StormSocketServerFrontendHttp::StormSocketServerFrontendHttp(const StormSocketServerFrontendHttpSettings & settings, StormSocketBackend * backend) :
    StormSocketFrontendHttpBase(settings, backend),
    m_ConnectionAllocator(sizeof(StormSocketServerConnectionHttp) * settings.MaxConnections, sizeof(StormSocketServerConnectionHttp), false),
    m_SSLData(std::make_unique<StormSocketServerSSLData[]>(kDefaultSSLConfigs)),
    m_HeaderValues()
  {
    for (int index = 0; index < kDefaultSSLConfigs; ++index)
    {
      m_UseSSL = InitServerSSL(settings.SSLSettings, m_SSLData[index]);
    }

    m_AcceptorId = m_Backend->InitAcceptor(this, settings.ListenSettings);
  }

  StormSocketServerFrontendHttp::~StormSocketServerFrontendHttp()
  {
    CleanupAllConnections();
    m_Backend->DestroyAcceptor(m_AcceptorId);

    if (m_UseSSL)
    {
      for (int index = 0; index < kDefaultSSLConfigs; ++index)
      {
        ReleaseServerSSL(m_SSLData[index]);
      }
    }
  }

#ifndef DISABLE_MBED
  mbedtls_ssl_config * StormSocketServerFrontendHttp::GetSSLConfig(StormSocketFrontendConnectionId frontend_id)
  {
    std::size_t slot = frontend_id.m_Index >= 0 ? frontend_id.m_Index : reinterpret_cast<std::size_t>(frontend_id.m_MallocBlock);
    return &m_SSLData[slot % kDefaultSSLConfigs].m_SSLConfig;
  }
#endif

  StormHttpResponseWriter StormSocketServerFrontendHttp::CreateOutgoingResponse(int response_code, const char * response_phrase)
  {
    return m_Backend->CreateHttpResponseWriter(response_code, response_phrase);
  }

  void StormSocketServerFrontendHttp::FinalizeOutgoingResponse(StormHttpResponseWriter & writer, bool write_content_length)
  {
    writer.FinalizeHeaders(write_content_length);
  }

  void StormSocketServerFrontendHttp::SendResponse(StormSocketConnectionId connection_id, StormHttpResponseWriter & writer)
  {
    m_Backend->SendHttpResponseToConnection(writer, connection_id);
  }

  void StormSocketServerFrontendHttp::FreeOutgoingResponse(StormHttpResponseWriter & writer)
  {
    m_Backend->FreeOutgoingHttpResponse(writer);
  }

  void StormSocketServerFrontendHttp::FreeIncomingRequest(StormHttpRequestReader & reader)
  {
    reader.FreeChain();
    m_Backend->DiscardReaderData(reader.m_ConnectionId, reader.m_FullDataLen);
  }

  StormSocketServerConnectionHttp & StormSocketServerFrontendHttp::GetHttpConnection(StormSocketFrontendConnectionId id)
  {
    StormSocketServerConnectionHttp * ptr = (StormSocketServerConnectionHttp *)m_ConnectionAllocator.ResolveHandle(id);
    new (ptr) StormSocketServerConnectionHttp();
    return *ptr;
  }

  StormSocketFrontendConnectionId StormSocketServerFrontendHttp::AllocateFrontendId()
  {
    StormFixedBlockHandle handle = m_ConnectionAllocator.AllocateBlock(StormFixedBlockType::Custom);
    if (handle == InvalidBlockHandle)
    {
      return InvalidFrontendId;
    }

    return handle;
  }

  void StormSocketServerFrontendHttp::FreeFrontendId(StormSocketFrontendConnectionId frontend_id)
  {
    auto & http_connection = GetHttpConnection(frontend_id);

    http_connection.~StormSocketServerConnectionHttp();
    m_ConnectionAllocator.FreeBlock(&http_connection, StormFixedBlockType::Custom);
  }

  void StormSocketServerFrontendHttp::InitConnection([[maybe_unused]] StormSocketConnectionId connection_id, 
    [[maybe_unused]] StormSocketFrontendConnectionId frontend_id, [[maybe_unused]] const void * init_data)
  {
    //auto & http_connection = GetHttpConnection(frontend_id);
  }

  void StormSocketServerFrontendHttp::CleanupConnection([[maybe_unused]] StormSocketConnectionId connection_id, 
    [[maybe_unused]] StormSocketFrontendConnectionId frontend_id)
  {
    //auto & http_connection = GetHttpConnection(frontend_id);
  }

  bool StormSocketServerFrontendHttp::ProcessData(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id)
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

          if (http_connection.m_GotRequestLine == false)
          {
            // Read the status line
            http_connection.m_RequestMethod = cur_header;

            if (ParseRequestLine(cur_header, http_connection) == false)
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

  static const uint32_t get_hash = crc32("GET");
  static const uint32_t head_hash = crc32("HEAD");
  static const uint32_t delete_hash = crc32("DELETE");
  static const uint32_t trace_hash = crc32("TRACE");

  bool StormSocketServerFrontendHttp::ParseRequestLine(StormMessageReaderCursor & request_line, StormSocketServerConnectionHttp & http_connection)
  {
    int header_val;
    int header_val_lowercase;

    StormMessageReaderCursor method_cursor = request_line;
    int method_len = 0;

    uint32_t method_hash = crc32begin();

    while (request_line.GetRemainingLength() > 0)
    {
      char c = request_line.PeekByte();
      if (c == ' ')
      {
        break;
      }

      method_hash = crc32additive(method_hash, c);

      method_len++;
      request_line.Advance(1);
    }

    method_hash = crc32end(method_hash);
    if (method_hash == get_hash ||
      method_hash == head_hash ||
      method_hash == delete_hash ||
      method_hash == trace_hash)
    {
      http_connection.m_BodyLength = 0;
    }

    http_connection.m_RequestMethod = StormMessageReaderCursor(method_cursor, method_len);
    if (request_line.GetRemainingLength() == 0 || request_line.ReadByte() != ' ')
    {
      return false;
    }

    StormMessageReaderCursor uri_cursor = request_line;
    int uri_len = 0;

    while (request_line.GetRemainingLength() > 0)
    {
      auto c = request_line.PeekByte();
      if (c == ' ')
      {
        break;
      }

      uri_len++;
      request_line.Advance(1);
    }

    http_connection.m_RequestURI = StormMessageReaderCursor(uri_cursor, uri_len);
    if (request_line.GetRemainingLength() == 0 || request_line.ReadByte() != ' ')
    {
      return false;
    }

    m_HeaderValues.ReadInitialVal(request_line, header_val, header_val_lowercase);
    if (m_HeaderValues.Match(request_line, header_val, StormHttpHeaderType::HttpVer) == false)
    {
      return false;
    }

    http_connection.m_GotRequestLine = true;
    return true;
  }

  void StormSocketServerFrontendHttp::AddBodyBlock(StormSocketConnectionId connection_id, StormHttpConnectionBase & http_connection_base, void * chunk_ptr, int chunk_len, int read_offset)
  {
    StormSocketServerConnectionHttp & http_connection = (StormSocketServerConnectionHttp &)http_connection_base;
    if (http_connection.m_BodyReader)
    {
      http_connection.m_BodyReader->AddBlock(chunk_ptr, chunk_len, read_offset);
    }
    else
    {
      http_connection.m_BodyReader =
        StormHttpRequestReader(chunk_ptr, chunk_len, read_offset, connection_id, &m_Allocator, &m_MessageReaders,
          *http_connection.m_RequestMethod, *http_connection.m_RequestURI, *http_connection.m_Headers);
    }
  }

  bool StormSocketServerFrontendHttp::CompleteBody(StormSocketConnectionId connection_id, StormHttpConnectionBase & http_connection_base)
  {
    StormSocketServerConnectionHttp & http_connection = (StormSocketServerConnectionHttp &)http_connection_base;
    auto & connection = GetConnection(connection_id);

    if (http_connection.m_BodyReader)
    {
      http_connection.m_BodyReader->FinalizeFullDataLength(http_connection.m_TotalLength);
    }
    else
    {
      http_connection.m_BodyReader =
        StormHttpRequestReader(nullptr, 0, 0, connection_id, &m_Allocator, &m_MessageReaders,
          *http_connection.m_RequestMethod, *http_connection.m_RequestURI, *http_connection.m_Headers);
    }

    StormSocketEventInfo data_message;
    data_message.ConnectionId = connection_id;
    data_message.GetHttpRequestReader() = *http_connection.m_BodyReader;
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

    http_connection.m_CompleteRequest = true;
    return true;
  }


  void StormSocketServerFrontendHttp::SendClosePacket([[maybe_unused]] StormSocketConnectionId connection_id, 
    [[maybe_unused]] StormSocketFrontendConnectionId frontend_id)
  {

  }
}
