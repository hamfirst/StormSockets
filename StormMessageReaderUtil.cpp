
#include "StormMessageReaderUtil.h"
#include "StormMessageReaderCursor.h"
#include "StormHttpBodyReader.h"


namespace StormSockets
{
  std::string ReadMessageAsString(StormMessageReaderCursor & cursor)
  {
    std::string str;
    str.resize(cursor.GetRemainingLength() + 1);
    cursor.ReadByteBlock((void *)str.data(), cursor.GetRemainingLength());
    return str;
  }


  std::string ReadMessageAsString(StormHttpBodyReader & body)
  {
    std::string str;
    str.resize(body.GetRemainingLength() + 1);
    body.ReadByteBlock((void *)str.data(), body.GetRemainingLength());
    return str;
  }
}
