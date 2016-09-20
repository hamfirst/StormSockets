#pragma once

#include <StormSockets\StormFixedBlockAllocator.h>

#include <atomic>

namespace StormSockets
{
  struct StormSocketBuffer
  {
    StormSocketBuffer();
    StormSocketBuffer(StormFixedBlockAllocator * allocator, int block_size);
    StormSocketBuffer(const StormSocketBuffer & rhs);
    StormSocketBuffer & operator = (const StormSocketBuffer & rhs);

    void InitBuffers();

    void GotData(int bytes_received);
    void FreeBuffers();
    void DiscardData(int amount);

  public:
    StormFixedBlockAllocator * m_Allocator;
    int m_FixedBlockSize;
    StormFixedBlockHandle m_BlockStart;
    StormFixedBlockHandle m_BlockCur;
    StormFixedBlockHandle m_BlockNext;

    int m_ReadOffset;
    int m_WriteOffset;
    std::atomic_int m_DataAvail;
  };
}
