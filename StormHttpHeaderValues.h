#pragma once

#include "StormMessageHeaderValues.h"

namespace StormSockets
{
  namespace StormHttpHeaderType
  {
    enum Index
    {
      HttpVer,
      ContentLength,
      ChunkedEncoding,
      Count,
    };
  }

  class StormHttpHeaderValues : public StormMessageHeaderValues
  {
  public:
    StormHttpHeaderValues();
  };
}

