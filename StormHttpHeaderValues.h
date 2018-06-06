#pragma once

#include "StormMessageHeaderValues.h"

namespace StormSockets
{
  namespace StormHttpHeaderType
  {
    enum Index
    {
      HttpVer,
      HttpVer1,
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

