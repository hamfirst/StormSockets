#pragma once

#include "StormMessageReaderCursor.h"
#include "StormMessageReaderData.h"

namespace StormSockets
{
  class StormHttpBodyReader
  {
    StormMessageReaderCursor m_Reader;

    StormFixedBlockAllocator * m_ReaderAllocator;
    StormMessageReaderData * m_PacketInfo;
    int m_FullDataLen;

    friend class StormHttpResponseReader;
    friend class StormHttpRequestReader;

  private:
    StormHttpBodyReader(StormMessageReaderData * packet_info, StormFixedBlockAllocator * allocator, StormFixedBlockAllocator * reader_allocator, int full_data_len);
    void Advance();

  public:
    StormHttpBodyReader() = default;
    StormHttpBodyReader(const StormHttpBodyReader & rhs) = default;
    StormHttpBodyReader(StormHttpBodyReader && rhs) = default;

    StormHttpBodyReader & operator = (const StormHttpBodyReader & rhs) = default;
    StormHttpBodyReader & operator = (StormHttpBodyReader && rhs) = default;

    uint8_t ReadByte();
    wchar_t ReadUTF8Char();
    uint16_t ReadInt16();
    uint32_t ReadInt32();
    uint64_t ReadInt64();

    void ReadByteBlock(void * buffer, int length);

    int GetRemainingLength() { return m_FullDataLen; }
  };


}
