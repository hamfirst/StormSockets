
#include "StormSocketClientFrontendWebsocket.h"

#include <random>
#include <chrono>

#include <hash/Hash.h>

void Log(const char * fmt, ...);

namespace StormSockets
{
  StormSocketClientFrontendWebsocket::StormSocketClientFrontendWebsocket(const StormSocketClientFrontendWebsocketSettings & settings, StormSocketBackend * backend) :
    StormSocketFrontendWebsocketBase(settings, backend),
    m_ConnectionAllocator(sizeof(StormSocketClientConnectionWebsocket) * settings.MaxConnections, sizeof(StormSocketClientConnectionWebsocket), false),
    m_SSLData(std::make_unique<StormSocketClientSSLData[]>(kDefaultSSLConfigs)),
    m_HeaderValues(nullptr)
  {
    for (int index = 0; index < kDefaultSSLConfigs; ++index)
    {
      InitClientSSL(m_SSLData[index], backend);
    }
  }

  StormSocketClientFrontendWebsocket::~StormSocketClientFrontendWebsocket()
  {
    CleanupAllConnections();
    for (int index = 0; index < kDefaultSSLConfigs; ++index)
    {
      ReleaseClientSSL(m_SSLData[index]);
    }
  }

  StormSocketConnectionId StormSocketClientFrontendWebsocket::RequestConnect(const char * ip_addr, int port,
    const StormSocketClientFrontendWebsocketRequestData & request_data)
  {
    return m_Backend->RequestConnect(this, ip_addr, port, &request_data);
  }

#ifndef DISABLE_MBED
  bool StormSocketClientFrontendWebsocket::UseSSL([[maybe_unused]] StormSocketConnectionId connection_id, 
    [[maybe_unused]] StormSocketFrontendConnectionId frontend_id)
  {
    auto & ws_connection = GetWSConnection(frontend_id);
    return ws_connection.m_UseSSL;
  }

  mbedtls_ssl_config * StormSocketClientFrontendWebsocket::GetSSLConfig(StormSocketFrontendConnectionId frontend_id)
  {
    std::size_t slot = frontend_id.m_Index >= 0 ? frontend_id.m_Index : reinterpret_cast<std::size_t>(frontend_id.m_MallocBlock);
    return &m_SSLData[slot % kDefaultSSLConfigs].m_SSLConfig;
  }

#endif

  StormSocketClientConnectionWebsocket & StormSocketClientFrontendWebsocket::GetWSConnection(StormSocketFrontendConnectionId id)
  {
    StormSocketClientConnectionWebsocket * ptr = (StormSocketClientConnectionWebsocket *)m_ConnectionAllocator.ResolveHandle(id);
    return *ptr;
  }

  StormSocketFrontendConnectionId StormSocketClientFrontendWebsocket::AllocateFrontendId()
  {
    StormFixedBlockHandle handle = m_ConnectionAllocator.AllocateBlock(StormFixedBlockType::Custom);
    if (handle == InvalidBlockHandle)
    {
      return InvalidFrontendId;
    }

    StormSocketClientConnectionWebsocket * ptr = (StormSocketClientConnectionWebsocket *)m_ConnectionAllocator.ResolveHandle(handle);
    new (ptr) StormSocketClientConnectionWebsocket();

    return handle;
  }

  void StormSocketClientFrontendWebsocket::FreeFrontendId(StormSocketFrontendConnectionId frontend_id)
  {
    auto & ws_connection = GetWSConnection(frontend_id);

    ws_connection.~StormSocketClientConnectionWebsocket();
    m_ConnectionAllocator.FreeBlock(&ws_connection, StormFixedBlockType::Custom);
  }

  void StormSocketClientFrontendWebsocket::InitConnection([[maybe_unused]] StormSocketConnectionId connection_id, 
    [[maybe_unused]] StormSocketFrontendConnectionId frontend_id, const void * init_data)
  {
    StormSocketClientFrontendWebsocketRequestData * request_data = (StormSocketClientFrontendWebsocketRequestData *)init_data;

    auto & ws_connection = GetWSConnection(frontend_id);
    ws_connection.m_UseSSL = request_data->m_UseSSL;

    ws_connection.m_Uri = request_data->m_Uri;
    ws_connection.m_Host = request_data->m_Host;

    if(request_data->m_Protocol) ws_connection.m_Protocol = request_data->m_Protocol;
    if(request_data->m_Origin) ws_connection.m_Origin = request_data->m_Origin;
  }

  void StormSocketClientFrontendWebsocket::CleanupConnection([[maybe_unused]] StormSocketConnectionId connection_id, 
    [[maybe_unused]] StormSocketFrontendConnectionId frontend_id)
  {
    auto & ws_connection = GetWSConnection(frontend_id);
    StormSocketFrontendWebsocketBase::CleanupWebsocketConnection(ws_connection);

    if (ws_connection.m_State == StormSocketServerConnectionWebsocketState::SendHandshakeResponse ||
      ws_connection.m_State == StormSocketServerConnectionWebsocketState::SendPong)
    {
      FreeOutgoingPacket(ws_connection.m_PendingWriter);
    }
  }

  bool StormSocketClientFrontendWebsocket::ProcessData(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id)
  {
    auto & connection = GetConnection(connection_id);
    auto & ws_connection = GetWSConnection(frontend_id);

    while (true)
    {
      // If we've already closed the connection, just ignore everything
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

      if (ws_connection.m_State == StormSocketServerConnectionWebsocketState::HandShake)
      {
        StormMessageHeaderReader header_reader(&m_Allocator, m_Allocator.ResolveHandle(connection.m_ParseBlock), connection.m_UnparsedDataLength, connection.m_ParseOffset);

        int full_data_len = 0;
        bool got_header = false;
        StormMessageReaderCursor cur_header = header_reader.AdvanceToNextHeader(full_data_len, got_header);

        if (!got_header)
        {
          return true;
        }

        while (got_header)
        {
          // Process header data
          if (cur_header.GetRemainingLength() == 0)
          {
            ws_connection.m_GotHeaderTerminator = true;
          }
          else
          {
            int header_val;
            int header_val_lowercase;

            m_HeaderValues.ReadInitialVal(cur_header, header_val, header_val_lowercase);

            if (ws_connection.m_GotStatusLineHeader == false &&
              m_HeaderValues.MatchCaseSensitive(cur_header, header_val, StormWebsocketHeaderType::StatusLine))
            {
              ws_connection.m_GotStatusLineHeader = true;
            }
            else if (ws_connection.m_GotWebsocketHeader == false &&
              m_HeaderValues.MatchExact(cur_header, header_val_lowercase, StormWebsocketHeaderType::WebsocketHeader))
            {
              ws_connection.m_GotWebsocketHeader = true;
            }
            else if (ws_connection.m_GotConnectionUpgradeHeader == false &&
              m_HeaderValues.Match(cur_header, header_val_lowercase, StormWebsocketHeaderType::ConnectionUpdgradeHeader))
            {
              if (m_HeaderValues.FindCSLValue(cur_header, StormWebsocketHeaderType::UpdgradePart))
              {
                ws_connection.m_GotConnectionUpgradeHeader = true;
              }
            }
            else if (ws_connection.m_Protocol.size() > 0 && ws_connection.m_GotWebsocketProtoHeader == false &&
              m_HeaderValues.Match(cur_header, header_val_lowercase, StormWebsocketHeaderType::WebsocketProtoResponseHeader))
            {
              uint32_t hash = cur_header.HashRemainingData(false);

              auto str_itr = ws_connection.m_Protocol.begin();

              bool found_hash = false;

              while(true)
              {
                // Skip initial whitespace
                while (*str_itr == ' ')
                {
                  ++str_itr;
                  if (str_itr == ws_connection.m_Protocol.end())
                  {
                    break;
                  }
                }

                uint32_t test_hash = crc32begin();
                while (str_itr != ws_connection.m_Protocol.end())
                {
                  char c = *str_itr;
                  ++str_itr;

                  if (c == ',')
                  {
                    break;
                  }

                  test_hash = crc32additive(test_hash, tolower(c));
                }

                test_hash = crc32end(test_hash);
                if (hash == test_hash)
                {
                  found_hash = true;
                  break;
                }
              }

              if (found_hash)
              {
                ws_connection.m_GotWebsocketProtoHeader = true;
              }
            }
            else if (ws_connection.m_GotWebsocketKeyHeader == false &&
              m_HeaderValues.Match(cur_header, header_val_lowercase, StormWebsocketHeaderType::WebsocketAcceptHeader))
            {
              cur_header.SkipWhiteSpace();
              if (cur_header.GetRemainingLength() >= 28)
              {
                bool matched = true;
                for (int index = 0; index < 28; index++)
                {
                  if (cur_header.ReadByte() != ws_connection.m_SecKeyHash[index])
                  {
                    matched = false;
                    break;
                  }
                }

                if (matched)
                {
                  ws_connection.m_GotWebsocketKeyHeader = true;
                }
              }
            }
          }

          m_Backend->DiscardReaderData(connection_id, full_data_len);
          m_Backend->DiscardParserData(connection_id, full_data_len);

          if (ws_connection.m_GotHeaderTerminator)
          {
            m_Backend->SetHandshakeComplete(connection_id);

            // Check to see if the connection is a valid websocket request
            if (ws_connection.m_GotStatusLineHeader &&
              ws_connection.m_GotWebsocketHeader &&
              ws_connection.m_GotConnectionUpgradeHeader &&
              ws_connection.m_GotWebsocketKeyHeader &&
              (ws_connection.m_Protocol.size() == 0 || ws_connection.m_GotWebsocketProtoHeader))
            {
              ws_connection.m_State = StormSocketServerConnectionWebsocketState::ReadHeaderAndApplyMask;
              QueueHandshakeCompleteEvent(connection_id, frontend_id);
            }
            else
            {
              ForceDisconnect(connection_id);
            }

            break;
          }

          cur_header = header_reader.AdvanceToNextHeader(full_data_len, got_header);
        }

        if (ws_connection.m_State == StormSocketServerConnectionWebsocketState::HandShake)
        {
          return true;
        }
      }

      return ProcessWebsocketData(connection, ws_connection, connection_id);
    }
  }

  void StormSocketClientFrontendWebsocket::ConnectionEstablishComplete(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id)
  {
    auto & ws_connection = GetWSConnection(frontend_id);
    auto writer = m_Backend->CreateWriter();

    auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    static std::mt19937_64 generator(seed);

    char sec_key[256];
    int offs = 0;

    for (int pass = 0; pass < 3; pass++)
    {
      uint64_t rand_val = generator();
      for (int index = 0; index < 8; index++)
      {
        int val = rand_val & 0x3F;
        rand_val >>= 6;

        if (val < 26)
        {
          sec_key[offs] = 'A' + val;
        }
        else if (val < 52)
        {
          sec_key[offs] = 'a' + val - 26;
        }
        else if (val < 62)
        {
          sec_key[offs] = '0' + val - 52;
        }
        else if (val == 63)
        {
          sec_key[offs] = '+';
        }
        else
        {
          sec_key[offs] = '/';
        }

        offs++;
      }
    }

    sec_key[offs - 1] = '=';
    sec_key[offs - 2] = '=';
    sec_key[offs] = 0;

    StormSha1::CalcHash(sec_key, ws_connection.m_SecKeyHash);

    writer.WriteByteBlock("GET ", 0, 4);
    writer.WriteByteBlock(ws_connection.m_Uri.c_str(), 0, ws_connection.m_Uri.size());
    writer.WriteByteBlock(" HTTP/1.1\r\nHost: ", 0, 20);
    writer.WriteByteBlock(ws_connection.m_Host.c_str(), 0, ws_connection.m_Host.size());

    if (ws_connection.m_Origin.size() > 0)
    {
      writer.WriteByteBlock("\r\nOrigin: ", 0, 10);
      writer.WriteByteBlock(ws_connection.m_Origin.c_str(), 0, ws_connection.m_Origin.size());
    }

    writer.WriteByteBlock("\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: ", 0, 89);
    writer.WriteByteBlock(sec_key, 0, offs);

    if (ws_connection.m_Protocol.size() > 0)
    {
      writer.WriteByteBlock("\r\nSec-WebSocket-Protocol: ", 0, 26);
      writer.WriteByteBlock(ws_connection.m_Protocol.c_str(), 0, ws_connection.m_Protocol.size());
    }

    writer.WriteByteBlock("\r\n\r\n", 0, 4);

    SendPacketToConnectionBlocking(writer, connection_id);
  }

  void StormSocketClientFrontendWebsocket::SendClosePacket(StormSocketConnectionId connection_id, [[maybe_unused]] StormSocketFrontendConnectionId frontend_id)
  {
    // Send a disconnect packet
    StormWebsocketMessageWriter disconnect_writer = CreateOutgoingPacket(StormWebsocketOp::Close, true);
    disconnect_writer.CreateHeaderAndApplyMask((int)StormWebsocketOp::Close, true, 0);
    SendPacketToConnection(disconnect_writer, connection_id);
    FreeOutgoingPacket(disconnect_writer);
  }
}