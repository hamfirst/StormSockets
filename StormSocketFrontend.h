#pragma once

#include "StormSocketConnectionId.h"
#include "StormMessageWriter.h"
#include "StormSocketConnection.h"
#include "StormSocketIOOperation.h"

namespace StormSockets
{
  static const StormSocketFrontendConnectionId InvalidFrontendId = InvalidBlockHandle;

  class StormSocketFrontend
  {
  public:

    virtual ~StormSocketFrontend() {};

#ifndef DISABLE_MBED
    virtual bool UseSSL(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id) = 0;
    virtual mbedtls_ssl_config * GetSSLConfig(StormSocketFrontendConnectionId frontend_id) = 0;
#endif

    virtual StormSocketFrontendConnectionId AllocateFrontendId() = 0;
    virtual void FreeFrontendId(StormSocketFrontendConnectionId frontend_id) = 0;

    virtual void AssociateConnectionId(StormSocketConnectionId connection_id) = 0;
    virtual void DisassociateConnectionId(StormSocketConnectionId connection_id) = 0;

    virtual void InitConnection(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id, const void * init_data) = 0;
    virtual void CleanupConnection(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id) = 0;

    virtual void QueueConnectEvent(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id, uint32_t remote_ip, uint16_t remote_port) = 0;
    virtual void QueueDisconnectEvent(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id) = 0;

    virtual void SendClosePacket(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id) = 0;

    virtual bool ProcessData(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id) = 0;
    virtual void ConnectionEstablishComplete(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id) = 0;
  };
}

