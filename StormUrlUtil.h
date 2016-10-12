#pragma once

#include <string>

namespace StormSockets
{
  struct StormURI
  {
    std::string m_Protocol;
    std::string m_Host;
    std::string m_Port;
    std::string m_Uri;
  };

  bool ParseURI(const char * uri, StormURI & out_uri);
}