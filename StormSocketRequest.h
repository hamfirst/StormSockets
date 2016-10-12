#pragma once

#include "StormHttpRequestWriter.h"

namespace StormSockets
{
  struct StormSocketClientFrontendHttpRequestData
  {
    StormHttpRequestWriter m_RequestWriter;
    bool m_UseSSL;
  };

  struct StormSocketClientFrontendWebsocketRequestData
  {
    bool m_UseSSL = false;
    const char * m_Uri = "/";
    const char * m_Host = "localhost";
    const char * m_Protocol = nullptr;
    const char * m_Origin = nullptr;
  };
}
