
#pragma once

#include "StormFixedBlockAllocator.h"

#include <atomic>
#include <cstdint>

namespace StormSockets
{
  namespace StormWebsocketOp
  {
    enum Index
    {
      Continuation = 0,
      TextFrame = 1,
      BinaryFrame = 2,
      Close = 8,
      Ping = 9,
      Pong = 10
    };
  }

  struct StormMessageWriterData
  {
    StormFixedBlockHandle m_StartBlock;
    StormFixedBlockHandle m_CurBlock;
    StormFixedBlockHandle m_PrevBlock;
    volatile int m_WriteOffset;
    volatile int m_TotalLength;
    volatile int m_SendOffset;
    std::atomic_int m_RefCount;
  };

  class StormMessageWriter
  {
  protected:
    StormMessageWriterData * m_PacketInfo;
    StormFixedBlockHandle m_PacketHandle;
    StormFixedBlockAllocator * m_Allocator;
    StormFixedBlockAllocator * m_SenderAllocator;
    bool m_IsEncrypted;

    int m_ReservedHeaderLength;
    int m_ReservedTrailerLength;
    int m_HeaderLength;
    int m_TrailerLength;

    friend class StormSocketBackend;

  protected:

    void Init(StormFixedBlockAllocator * block_allocator, StormFixedBlockAllocator * sender_allocator, bool encrypted, int header_length, int trailer_length);

  public:

    int GetLength();
    void DebugPrint();

    void WriteByte(uint8_t b);
    void WriteUTF8Char(wchar_t c);
    void WriteInt16(uint16_t s);
    void WriteInt32(uint32_t i);
    void WriteInt64(uint64_t l);
    void WriteByteBlock(const void * buffer, int start_offset, std::size_t length);
    void RemoveBytes(int length);

    void WriteString(const char * str);

    uint8_t * GetCurrentWriteAddress();
    void AdvanceByte();
  };
}

