#pragma once

#include "StormSocketFrontendWebsocketBase.h"
#include "StormSocketConnectionWebsocket.h"
#include "StormWebsocketHeaderValues.h"
#include "StormWebsocketMessageWriter.h"
#include "StormSha1.h"

namespace StormSockets
{
  class StormSocketServerFrontendWebsocket : public StormSocketFrontendWebsocketBase
  {
  protected:
    StormFixedBlockAllocator m_ConnectionAllocator;
    std::unique_ptr<StormSocketServerSSLData[]> m_SSLData;
    bool m_UseSSL;
    int m_Port;

    StormSocketBackendAcceptorId m_AcceptorId;

    StormWebsocketHeaderValues m_HeaderValues;
    bool m_HasProtocol;

    int m_MaxHeaderSize;

  public:

    StormSocketServerFrontendWebsocket(const StormSocketServerFrontendWebsocketSettings & settings, StormSocketBackend * backend);
    ~StormSocketServerFrontendWebsocket();

#ifndef DISABLE_MBED
    bool UseSSL(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id) { return m_UseSSL; }
    mbedtls_ssl_config * GetSSLConfig(StormSocketFrontendConnectionId frontend_id);
#endif

  protected:

    StormSocketServerConnectionWebSocket & GetWSConnection(StormSocketFrontendConnectionId id);

    StormSocketFrontendConnectionId AllocateFrontendId();
    void FreeFrontendId(StormSocketFrontendConnectionId frontend_id);

    void InitConnection(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id, const void * init_data);
    void CleanupConnection(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);
    bool ProcessData(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);

    void SendClosePacket(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);
    void ConnectionEstablishComplete(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id) { }
  };
}

