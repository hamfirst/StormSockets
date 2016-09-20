
#include "StormSocketServerWebsocket.h"
#include "StormSocketBackend.h"
#include "StormSocketServerFrontendWebsocket.h"

namespace StormSockets
{
  using BackendType = StormSocketBackend;
  using FrontendType = StormSocketServerFrontendWebsocket;

  StormSocketServerWebsocket::StormSocketServerWebsocket(StormSocketInitSettings & backend_settings, StormSocketServerFrontendWebsocketSettings & server_settings)
  {
    BackendType * backend = new BackendType(backend_settings);
    FrontendType * frontend = new FrontendType(server_settings, backend);
    m_Frontend = (StormSocketServerImpl)frontend;
    m_Backend = (StormSocketServerImpl)backend;
  }

  StormSocketServerWebsocket::StormSocketServerWebsocket(StormSocketServerWebsocket && rhs) noexcept
  {
    m_Frontend = rhs.m_Frontend;
    m_Backend = rhs.m_Backend;
    rhs.m_Frontend = nullptr;
    rhs.m_Backend = nullptr;
  }

  StormSocketServerWebsocket::~StormSocketServerWebsocket()
  {
    if (m_Frontend)
    {
      FrontendType * server = (FrontendType *)m_Frontend;
      delete server;
    }

    if (m_Backend)
    {
      BackendType * server = (BackendType *)m_Backend;
      delete server;
    }
  }

  StormSocketServerWebsocket & StormSocketServerWebsocket::operator = (StormSocketServerWebsocket && rhs) noexcept
  {
    m_Frontend = rhs.m_Frontend;
    m_Backend = rhs.m_Backend;
    rhs.m_Frontend = nullptr;
    rhs.m_Backend = nullptr;
    return *this;
  }

  bool StormSocketServerWebsocket::GetEvent(StormSocketEventInfo & message)
  {
    FrontendType * server = (FrontendType *)m_Frontend;
    return server->GetEvent(message);
  }

  StormWebsocketMessageWriter StormSocketServerWebsocket::CreateOutgoingPacket(StormSocketWebsocketDataType::Index type, bool final)
  {
    FrontendType * server = (FrontendType *)m_Frontend;
    return server->CreateOutgoingPacket(type, final);
  }

  void StormSocketServerWebsocket::FinalizeOutgoingPacket(StormWebsocketMessageWriter & writer)
  {
    FrontendType * server = (FrontendType *)m_Frontend;
    return server->FinalizeOutgoingPacket(writer);
  }

  bool StormSocketServerWebsocket::SendPacketToConnection(StormWebsocketMessageWriter & writer, StormSocketConnectionId id)
  {
    FrontendType * server = (FrontendType *)m_Frontend;
    return server->SendPacketToConnection(writer, id);
  }

  void StormSocketServerWebsocket::SendPacketToConnectionBlocking(StormWebsocketMessageWriter & writer, StormSocketConnectionId id)
  {
    FrontendType * server = (FrontendType *)m_Frontend;
    return server->SendPacketToConnectionBlocking(writer, id);
  }

  void StormSocketServerWebsocket::FreeOutgoingPacket(StormWebsocketMessageWriter & writer)
  {
    FrontendType * server = (FrontendType *)m_Frontend;
    return server->FreeOutgoingPacket(writer);
  }

  void StormSocketServerWebsocket::FreeIncomingPacket(StormWebsocketMessageReader & reader)
  {
    FrontendType * server = (FrontendType *)m_Frontend;
    return server->FreeIncomingPacket(reader);
  }

  void StormSocketServerWebsocket::FinalizeConnection(StormSocketConnectionId id)
  {
    FrontendType * server = (FrontendType *)m_Frontend;
    return server->FinalizeConnection(id);
  }

  void StormSocketServerWebsocket::ForceDisconnect(StormSocketConnectionId id)
  {
    FrontendType * server = (FrontendType *)m_Frontend;
    return server->ForceDisconnect(id);
  }
}
