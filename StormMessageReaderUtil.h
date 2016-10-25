#pragma once

#include <string>

namespace StormSockets
{
  struct StormMessageReaderCursor;
  class StormHttpBodyReader;

  std::string ReadMessageAsString(StormMessageReaderCursor & cursor);
  std::string ReadMessageAsString(StormHttpBodyReader & body);
}