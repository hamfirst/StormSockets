
#include "StormHttpHeaderValues.h"

#include <stdexcept>

namespace StormSockets
{
  StormHttpHeaderValues::StormHttpHeaderValues()
  {
    std::vector<std::string> strs;

    strs.push_back("HTTP/1.1");
    strs.push_back("HTTP/1.0");
    strs.push_back("content-length: ");
    strs.push_back("transfer-encoding: chunked");

    if (strs.size() != (int)StormHttpHeaderType::Count)
    {
      throw std::runtime_error("error: header matcher array is inconsistent");
    }

    Init(strs);
  }
}