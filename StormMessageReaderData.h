#pragma once

namespace StormSockets
{
  struct StormMessageReaderData
  {
    void * m_CurBlock;
    int m_DataLength;
    int m_ReadOffset;
  };
}
