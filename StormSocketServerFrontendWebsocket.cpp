
#include "StormSocketServerFrontendWebsocket.h"

namespace StormSockets
{
  StormSocketServerFrontendWebsocket::StormSocketServerFrontendWebsocket(
    const StormSocketServerFrontendWebsocketSettings & settings, StormSocketBackend * backend) :
    StormSocketFrontendWebsocketBase(settings, backend),
    m_ConnectionAllocator(sizeof(StormSocketServerConnectionWebSocket) * settings.MaxConnections, sizeof(StormSocketServerConnectionWebSocket), false),
    m_SSLData(std::make_unique<StormSocketServerSSLData[]>(kDefaultSSLConfigs)),
    m_HeaderValues(settings.Protocol),
    m_MaxHeaderSize(settings.MaxHeaderSize)
  {
    for (int index = 0; index < kDefaultSSLConfigs; ++index)
    {
      m_UseSSL = InitServerSSL(settings.SSLSettings, m_SSLData[index]);
    }

    m_HasProtocol = settings.Protocol != NULL;

    m_AcceptorId = m_Backend->InitAcceptor(this, settings.ListenSettings);
    m_Port = settings.ListenSettings.Port;
  }

  StormSocketServerFrontendWebsocket::~StormSocketServerFrontendWebsocket()
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
  mbedtls_ssl_config * StormSocketServerFrontendWebsocket::GetSSLConfig(StormSocketFrontendConnectionId frontend_id)
  {
    std::size_t slot = frontend_id.m_Index >= 0 ? frontend_id.m_Index : reinterpret_cast<std::size_t>(frontend_id.m_MallocBlock);
    return &m_SSLData[slot % kDefaultSSLConfigs].m_SSLConfig;
  }
#endif

  StormSocketServerConnectionWebSocket & StormSocketServerFrontendWebsocket::GetWSConnection(StormSocketFrontendConnectionId id)
  {
    StormSocketServerConnectionWebSocket * ptr = (StormSocketServerConnectionWebSocket *)m_ConnectionAllocator.ResolveHandle(id);
    return *ptr;
  }

  StormSocketFrontendConnectionId StormSocketServerFrontendWebsocket::AllocateFrontendId()
  {
    StormFixedBlockHandle handle = m_ConnectionAllocator.AllocateBlock(StormFixedBlockType::Custom);
    if (handle == InvalidBlockHandle)
    {
      return InvalidFrontendId;
    }


    StormSocketServerConnectionWebSocket * ptr = (StormSocketServerConnectionWebSocket *)m_ConnectionAllocator.ResolveHandle(handle);
    new (ptr) StormSocketServerConnectionWebSocket();

    return handle;
  }

  void StormSocketServerFrontendWebsocket::FreeFrontendId(StormSocketFrontendConnectionId frontend_id)
  {
    auto & ws_connection = GetWSConnection(frontend_id);

    ws_connection.~StormSocketServerConnectionWebSocket();
    m_ConnectionAllocator.FreeBlock(&ws_connection, StormFixedBlockType::Custom);
  }

  void StormSocketServerFrontendWebsocket::InitConnection([[maybe_unused]] StormSocketConnectionId connection_id, 
    [[maybe_unused]] StormSocketFrontendConnectionId frontend_id, [[maybe_unused]] const void * init_data)
  {
    auto & ws_connection = GetWSConnection(frontend_id);

    // Set up the handshake response
    ws_connection.m_PendingWriter = m_Backend->CreateWriter();
    m_HeaderValues.WriteHeader(ws_connection.m_PendingWriter, StormWebsocketHeaderType::Response);
  }

  void StormSocketServerFrontendWebsocket::CleanupConnection([[maybe_unused]] StormSocketConnectionId connection_id, 
    [[maybe_unused]] StormSocketFrontendConnectionId frontend_id)
  {
    auto & ws_connection = GetWSConnection(frontend_id);
    StormSocketFrontendWebsocketBase::CleanupWebsocketConnection(ws_connection);

    if (ws_connection.m_State == StormSocketServerConnectionWebsocketState::HandShake ||
        ws_connection.m_State == StormSocketServerConnectionWebsocketState::SendHandshakeResponse ||
        ws_connection.m_State == StormSocketServerConnectionWebsocketState::SendPong)
    {
      FreeOutgoingPacket(ws_connection.m_PendingWriter);
    }
  }

  bool StormSocketServerFrontendWebsocket::ProcessData(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id)
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

            if (ws_connection.m_GotGetHeader == false &&
              m_HeaderValues.MatchExactCaseSensitive(cur_header, header_val, StormWebsocketHeaderType::GetHeader))
            {
              ws_connection.m_GotGetHeader = true;
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
            else if (ws_connection.m_GotWebsocketVerHeader == false &&
              m_HeaderValues.MatchExact(cur_header, header_val_lowercase, StormWebsocketHeaderType::WebsocketVerHeader))
            {
              ws_connection.m_GotWebsocketVerHeader = true;
            }
            else if (m_HasProtocol && ws_connection.m_GotWebsocketProtoHeader == false &&
              m_HeaderValues.Match(cur_header, header_val_lowercase, StormWebsocketHeaderType::WebsocketProtoHeader))
            {
              ws_connection.m_GotWebsocketProtoHeader = true;
            }
            else if (ws_connection.m_GotWebsocketKeyHeader == false &&
              m_HeaderValues.Match(cur_header, header_val_lowercase, StormWebsocketHeaderType::WebsocketKeyHeader))
            {
              StormSha1::CalcHash(cur_header, ws_connection.m_PendingWriter);
              ws_connection.m_GotWebsocketKeyHeader = true;
            }
          }

          m_Backend->DiscardReaderData(connection_id, full_data_len);
          m_Backend->DiscardParserData(connection_id, full_data_len);

          if (ws_connection.m_GotHeaderTerminator)
          {
            // Check to see if the connection is a valid websocket request
            if (ws_connection.m_GotGetHeader &&
              ws_connection.m_GotWebsocketHeader &&
              ws_connection.m_GotConnectionUpgradeHeader &&
              ws_connection.m_GotWebsocketVerHeader &&
              ws_connection.m_GotWebsocketKeyHeader &&
              (m_HasProtocol == false || ws_connection.m_GotWebsocketProtoHeader))
            {
              m_HeaderValues.WriteHeader(ws_connection.m_PendingWriter, StormWebsocketHeaderType::ResponseTerminator);
              ws_connection.m_State = StormSocketServerConnectionWebsocketState::SendHandshakeResponse;

              m_Backend->SetHandshakeComplete(connection_id);
            }
            else
            {
              ForceDisconnect(connection_id);
            }

            break;
          }

          ws_connection.m_ProcessedHeaderSize += full_data_len;
          if (m_MaxHeaderSize > 0 && m_MaxHeaderSize < ws_connection.m_ProcessedHeaderSize)
          {
            ForceDisconnect(connection_id);
            break;
          }

          cur_header = header_reader.AdvanceToNextHeader(full_data_len, got_header);
        }

        if (ws_connection.m_State == StormSocketServerConnectionWebsocketState::HandShake)
        {
          return true;
        }
      }

      if (ws_connection.m_State == StormSocketServerConnectionWebsocketState::SendHandshakeResponse)
      {
        if (SendPacketToConnection(ws_connection.m_PendingWriter, connection_id) == false)
        {
          return false;
        }

        ws_connection.m_State = StormSocketServerConnectionWebsocketState::ReadHeaderAndApplyMask;
        QueueHandshakeCompleteEvent(connection_id, frontend_id);
      }

      return ProcessWebsocketData(connection, ws_connection, connection_id);
    }
  }

  void StormSocketServerFrontendWebsocket::SendClosePacket(StormSocketConnectionId connection_id, 
    [[maybe_unused]] StormSocketFrontendConnectionId frontend_id)
  {
    // Send a disconnect packet
    StormWebsocketMessageWriter disconnect_writer = CreateOutgoingPacket(StormWebsocketOp::Close, true);
    disconnect_writer.CreateHeaderAndApplyMask((int)StormWebsocketOp::Close, true, 0);
    SendPacketToConnection(disconnect_writer, connection_id);
    FreeOutgoingPacket(disconnect_writer);
  }
}
