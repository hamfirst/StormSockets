#pragma once

#include "StormMessageWriter.h"

namespace StormSockets
{
  class StormWebsocketMessageWriter : public StormMessageWriter
  {
  protected:

    bool m_Final;
    StormWebsocketOp::Index m_Mode;

    friend class StormSocketServerFrontendWebsocket;
    friend class StormSocketClientFrontendWebsocket;
    friend class StormSocketFrontendWebsocketBase;

  public:
    StormWebsocketMessageWriter() = default;
    StormWebsocketMessageWriter(const StormWebsocketMessageWriter & rhs) = default;

  protected:
    StormWebsocketMessageWriter(const StormMessageWriter & writer);

    void SaveHeaderRoom();
    void CreateHeaderAndApplyMask(int opcode, bool fin, int mask);

    static const int WebsocketMaxHeaderSize = 14;

  };
}

