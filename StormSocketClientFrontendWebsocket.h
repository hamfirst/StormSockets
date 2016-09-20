#pragma once

#include "StormSocketFrontendWebsocketBase.h"
#include "StormSocketConnectionWebsocket.h"
#include "StormWebsocketHeaderValues.h"
#include "StormWebsocketMessageWriter.h"
#include "StormSha1.h"

namespace StormSockets
{
  struct StormSocketClientFrontendWebsocketRequestData
  {
    bool m_UseSSL;
    const char * m_Uri;
    const char * m_Host;
    const char * m_Protocol;
    const char * m_Origin;
  };


  class StormSocketClientFrontendWebsocket : public StormSocketFrontendWebsocketBase
  {
  protected:
    StormFixedBlockAllocator m_ConnectionAllocator;
    StormSocketClientSSLData m_SSLData;

    StormWebsocketHeaderValues m_HeaderValues;

  public:

    StormSocketClientFrontendWebsocket(StormSocketClientFrontendWebsocketSettings & settings, StormSocketBackend * backend);
    ~StormSocketClientFrontendWebsocket();

#ifdef USE_MBED
    bool UseSSL(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);
    mbedtls_ssl_config * GetSSLConfig() { return &m_SSLData.m_SSLConfig; }
#endif

  protected:

    StormSocketClientConnectionWebsocket & GetWSConnection(StormSocketFrontendConnectionId id);

    StormSocketFrontendConnectionId AllocateFrontendId();
    void FreeFrontendId(StormSocketFrontendConnectionId frontend_id);

    void InitConnection(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id, void * init_data);
    void CleanupConnection(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);

    bool ProcessData(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);

    void ConnectionEstablishComplete(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);
    void SendClosePacket(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);
  };
}

