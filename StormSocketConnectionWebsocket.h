#pragma once

#include "StormSocketConnection.h"


namespace StormSockets
{
  namespace StormSocketServerConnectionWebsocketState
  {
    enum Index
    {
      HandShake,
      SendHandshakeResponse,
      ReadHeaderAndApplyMask,
      HandleContinuations,
      HandleIncomingPacket,
      SendPong,
    };
  }

  struct StormWebsocketConnectionBase
  {
    StormSocketServerConnectionWebsocketState::Index m_State = StormSocketServerConnectionWebsocketState::HandShake;

    bool m_InContinuation = false;
    StormMessageWriter m_PendingWriter;

    StormWebsocketMessageReader m_InitialReader;
    StormWebsocketMessageReader m_LastReader;
    StormWebsocketMessageReader m_PendingReader;

    int m_PendingReaderFullPacketLen = 0;
    bool m_ReaderValid = false;
  };

  struct StormSocketServerConnectionWebSocket : public StormWebsocketConnectionBase
  {
    bool m_GotGetHeader = false;
    bool m_GotWebsocketHeader = false;
    bool m_GotConnectionUpgradeHeader = false;
    bool m_GotWebsocketKeyHeader = false;
    bool m_GotWebsocketVerHeader = false;
    bool m_GotWebsocketProtoHeader = false;
    bool m_GotHeaderTerminator = false;
    int m_ProcessedHeaderSize = 0;
  };

  struct StormSocketClientConnectionWebsocket : public StormWebsocketConnectionBase
  {
    bool m_UseSSL = false;
    std::string m_Uri;
    std::string m_Host;
    std::string m_Protocol;
    std::string m_Origin;

    std::string m_SecKeyHash;

    bool m_GotStatusLineHeader = false;
    bool m_GotWebsocketHeader = false;
    bool m_GotConnectionUpgradeHeader = false;
    bool m_GotWebsocketKeyHeader = false;
    bool m_GotWebsocketProtoHeader = false;
    bool m_GotHeaderTerminator = false;
  };
}
