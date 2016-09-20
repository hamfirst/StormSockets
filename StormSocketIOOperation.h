#pragma once

#include "StormSocketConnectionId.h"
#include "StormMessageWriter.h"

namespace StormSockets
{
  namespace StormSocketIOOperationType
  {
    enum Index
    {
      SendPacket,
      FreePacket,
      ClearQueue,
      Close,
    };
  }

  struct StormSocketIOOperation
  {
    StormSocketIOOperationType::Index m_Type;
    StormSocketConnectionId m_ConnectionId;
    int m_Size;
  };

  struct StormSocketFreeQueueElement
  {
    StormMessageWriter m_RequestWriter;
  };

}
