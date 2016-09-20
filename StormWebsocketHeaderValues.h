#pragma once

#include "StormMessageHeaderValues.h"

namespace StormSockets
{
  namespace StormWebsocketHeaderType
  {
    enum Index
    {
      GetHeader,
      WebsocketHeader,
      ConnectionUpdgradeHeader,
      UpdgradePart,
      WebsocketKeyHeader,
      WebsocketVerHeader,
      WebsocketProtoHeader,
      Response,
      StatusLine,
      WebsocketAcceptHeader,
      WebsocketProtoResponseHeader,
      ResponseTerminator,
      Count,
    };
  }

  class StormWebsocketHeaderValues : public StormMessageHeaderValues
  {
  public:
    StormWebsocketHeaderValues(const char * protocol);
  };
}

