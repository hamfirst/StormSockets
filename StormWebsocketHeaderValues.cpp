#include "StormWebsocketHeaderValues.h"

#include <stdexcept>
#include <algorithm>

namespace StormSockets
{
  StormWebsocketHeaderValues::StormWebsocketHeaderValues(const char * protocol) :
    StormMessageHeaderValues()
  {
    std::vector<std::string> strs;

    strs.push_back("GET / HTTP/1.1");
    strs.push_back("upgrade: websocket");
    strs.push_back("connection: ");
    strs.push_back("upgrade");
    strs.push_back("sec-websocket-key: ");
    strs.push_back("sec-websocket-version: 13");

    if (protocol != NULL)
    {
      strs.push_back(std::string("sec-websocket-protocol: ") + std::string(protocol));
      std::transform(strs.back().begin(), strs.back().end(), strs.back().begin(), ::tolower);

      strs.push_back(std::string("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Protocol: ") +
        std::string(protocol) + std::string("\r\nSec-WebSocket-Accept: "));
    }
    else
    {
      strs.push_back("sec-websocket-protocol: ");
      strs.push_back("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ");
    }

    strs.push_back("HTTP/1.1 101 ");
    strs.push_back("sec-websocket-accept: ");
    strs.push_back("sec-websocket-protocol: ");
    strs.push_back("\r\n\r\n");

    if (strs.size() != (int)StormWebsocketHeaderType::Count)
    {
      throw std::runtime_error("error: header matcher array is inconsistent");
    }

    Init(strs);
  }
}
