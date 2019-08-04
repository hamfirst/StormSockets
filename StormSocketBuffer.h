#pragma once

#include "StormFixedBlockAllocator.h"
#include "StormMutex.h"

#include <atomic>

namespace StormSockets
{
  struct StormSocketBufferWriteInfo
  {
    void * m_Ptr1;
    std::size_t m_Ptr1Size;
    void * m_Ptr2;
    std::size_t m_Ptr2Size;
  };

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
    int GetDataAvailable() const;

	  int BlockRead(void * buffer, int size);
    bool GetPointerInfo(StormSocketBufferWriteInfo & pointer_info);

  public:
    StormFixedBlockAllocator * m_Allocator;
    int m_FixedBlockSize;
    StormFixedBlockHandle m_BlockStart;
	  StormFixedBlockHandle m_BlockCur;
	  StormFixedBlockHandle m_BlockNext;

    int m_ReadOffset;
    int m_WriteOffset;
    std::atomic_int m_DataAvail;
    std::atomic_int m_FreeSpaceAvail;
    std::atomic_bool m_InUse;

    StormMutex m_Mutex;
  };
}
