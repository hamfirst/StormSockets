#pragma once

#include "StormFixedBlockAllocator.h"


#include <stdint.h>

namespace StormSockets
{
  struct StormMessageReaderCursor
  {
  protected:
    StormFixedBlockAllocator * m_Allocator;
    void * m_CurBlock;
    int m_DataLength;
    int m_ReadOffset;
    int m_FixedBlockSize;

    friend class StormWebsocketMessageReader;
    friend class StormHttpBodyReader;

  public:
    StormMessageReaderCursor() = default;
    StormMessageReaderCursor(StormFixedBlockAllocator * allocator, void * cur_block, int data_length, int read_offset);
    StormMessageReaderCursor(const StormMessageReaderCursor & rhs, int length);
    StormMessageReaderCursor(const StormMessageReaderCursor & rhs) = default;

    int GetRemainingLength() const { return m_DataLength; }
    
    uint8_t PeekByte();
    uint8_t ReadByte();
    uint16_t ReadInt16();
    uint32_t ReadInt32();
    uint64_t ReadInt64();

    void ReadByteBlock(void * buffer, int length);

    void SkipWhiteSpace();

    bool ReadNumber(int & value, int required_digits = -1);
    bool ReadHexNumber(int & value, int required_digits = -1);

    uint32_t HashRemainingData(bool include_spaces);
    uint32_t HashUntilDelimiter(char delimiter);

    void Advance(int num_bytes);
  };
}
