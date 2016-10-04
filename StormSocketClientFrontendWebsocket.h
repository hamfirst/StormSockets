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
    bool m_UseSSL = false;
    const char * m_Uri = "/";
    const char * m_Host = "localhost";
    const char * m_Protocol = nullptr;
    const char * m_Origin = nullptr;
  };


  class StormSocketClientFrontendWebsocket : public StormSocketFrontendWebsocketBase
  {
  protected:
    StormFixedBlockAllocator m_ConnectionAllocator;
    StormSocketClientSSLData m_SSLData;

    StormWebsocketHeaderValues m_HeaderValues;

  public:

    StormSocketClientFrontendWebsocket(const StormSocketClientFrontendWebsocketSettings & settings, StormSocketBackend * backend);
    ~StormSocketClientFrontendWebsocket();

    StormSocketConnectionId RequestConnect(const char * ip_addr, int port, const StormSocketClientFrontendWebsocketRequestData & request_data);

#ifdef USE_MBED
    bool UseSSL(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);
    mbedtls_ssl_config * GetSSLConfig() { return &m_SSLData.m_SSLConfig; }
#endif

  protected:

    StormSocketClientConnectionWebsocket & GetWSConnection(StormSocketFrontendConnectionId id);

    StormSocketFrontendConnectionId AllocateFrontendId();
    void FreeFrontendId(StormSocketFrontendConnectionId frontend_id);

    void InitConnection(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id, const void * init_data);
    void CleanupConnection(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);

    bool ProcessData(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);

    void ConnectionEstablishComplete(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);
    void SendClosePacket(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);
  };
}

