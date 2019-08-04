#pragma once

#include "StormSocketFrontendBase.h"
#include "StormSocketConnectionWebsocket.h"
#include "StormWebsocketMessageWriter.h"
#include "StormWebsocketMessageReader.h"

namespace StormSockets
{
  class StormSocketFrontendWebsocketBase : public StormSocketFrontendBase
  {
  protected:

    bool m_UseMasking;
    StormSocketContinuationMode::Index m_ContinuationMode;
    int m_MaxPacketSize;

  public:

    StormSocketFrontendWebsocketBase(const StormSocketFrontendWebsocketSettings & settings, StormSocketBackend * backend);

    StormWebsocketMessageWriter CreateOutgoingPacket(StormSocketWebsocketDataType::Index type, bool final);
    void FinalizeOutgoingPacket(StormWebsocketMessageWriter & writer);
    void FreeIncomingPacket(StormWebsocketMessageReader & reader);

  protected:

    StormWebsocketMessageWriter CreateOutgoingPacket(StormWebsocketOp::Index mode, bool final);
    bool ProcessWebsocketData(StormSocketConnectionBase & connection, StormWebsocketConnectionBase & ws_connection, StormSocketConnectionId connection_id);

    void CleanupWebsocketConnection(StormWebsocketConnectionBase & ws_connection);
  };
}
