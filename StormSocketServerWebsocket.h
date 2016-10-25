
#pragma once

#include "StormSocketServerTypes.h"
#include "StormWebsocketMessageWriter.h"


namespace StormSockets
{
  using StormSocketServerImpl = void *;

  class StormSocketServerWebsocket
  {
  public:
    StormSocketServerWebsocket(StormSocketInitSettings & backend_settings, StormSocketServerFrontendWebsocketSettings & server_settings);
    StormSocketServerWebsocket(StormSocketServerWebsocket && rhs) noexcept;
    StormSocketServerWebsocket(const StormSocketServerWebsocket & rhs) = delete;
    ~StormSocketServerWebsocket();

    StormSocketServerWebsocket & operator = (StormSocketServerWebsocket && rhs) noexcept;

    bool GetEvent(StormSocketEventInfo & message);

    StormWebsocketMessageWriter CreateOutgoingPacket(StormSocketWebsocketDataType::Index type, bool final);
    void FinalizeOutgoingPacket(StormWebsocketMessageWriter & writer);

    bool SendPacketToConnection(StormWebsocketMessageWriter & writer, StormSocketConnectionId id);
    void SendPacketToConnectionBlocking(StormWebsocketMessageWriter & writer, StormSocketConnectionId id);

    void FreeOutgoingPacket(StormWebsocketMessageWriter & writer);
    void FreeIncomingPacket(StormWebsocketMessageReader & reader);
    void FinalizeConnection(StormSocketConnectionId id);
    void ForceDisconnect(StormSocketConnectionId id);

  private:

    StormSocketServerImpl m_Frontend;
    StormSocketServerImpl m_Backend;
  };
}



