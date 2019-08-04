
#include "StormSocketBuffer.h"
#include "StormMemOps.h"

#include <stdexcept>
#include <algorithm>
#include <cstring>

namespace StormSockets
{
  StormSocketBuffer::StormSocketBuffer()
  {
    m_Allocator = nullptr;
    m_FixedBlockSize = 0;
    m_BlockStart = InvalidBlockHandle;
    m_BlockCur = InvalidBlockHandle;
    m_BlockNext = InvalidBlockHandle;
    m_ReadOffset = 0;
    m_WriteOffset = 0;
    m_DataAvail = 0;
    m_InUse = 0;
    m_FreeSpaceAvail = 0;
  }

  StormSocketBuffer::StormSocketBuffer(StormFixedBlockAllocator * allocator, int block_size)
  {
    m_Allocator = allocator;
    m_FixedBlockSize = block_size;
    m_BlockStart = InvalidBlockHandle;
    m_BlockCur = InvalidBlockHandle;
    m_BlockNext = InvalidBlockHandle;
    m_ReadOffset = 0;
    m_WriteOffset = 0;
    m_DataAvail = 0;
    m_InUse = 0;
    m_FreeSpaceAvail = 0;
  }

  StormSocketBuffer::StormSocketBuffer(const StormSocketBuffer & rhs)
  {
    m_Allocator = rhs.m_Allocator;
    m_FixedBlockSize = rhs.m_FixedBlockSize;
    m_BlockStart = rhs.m_BlockStart;
    m_BlockCur = rhs.m_BlockCur;
    m_BlockNext = rhs.m_BlockNext;
    m_ReadOffset = rhs.m_ReadOffset;
    m_WriteOffset = rhs.m_WriteOffset;
    m_DataAvail = (int)rhs.m_DataAvail;
    m_InUse = (bool)rhs.m_InUse;
    m_FreeSpaceAvail = (int)rhs.m_FreeSpaceAvail;
  }

  StormSocketBuffer & StormSocketBuffer::operator = (const StormSocketBuffer & rhs)
  {
    m_Allocator = rhs.m_Allocator;
    m_FixedBlockSize = rhs.m_FixedBlockSize;
    m_BlockStart = rhs.m_BlockStart;
    m_BlockCur = rhs.m_BlockCur;
    m_BlockNext = rhs.m_BlockNext;
    m_ReadOffset = rhs.m_ReadOffset;
    m_WriteOffset = rhs.m_WriteOffset;
    m_DataAvail = (int)rhs.m_DataAvail;
    m_InUse = (bool)rhs.m_InUse;
    m_FreeSpaceAvail = (int)rhs.m_FreeSpaceAvail;
    return *this;
  }

  void StormSocketBuffer::InitBuffers()
  {
    StormUniqueLock<StormMutex> lock(m_Mutex);
    m_BlockStart = m_Allocator->AllocateBlock(StormFixedBlockType::BlockMem);
    m_BlockCur = m_BlockStart;
    m_BlockNext = m_Allocator->AllocateBlock(m_BlockCur, StormFixedBlockType::BlockMem);
    m_FreeSpaceAvail = m_FixedBlockSize * 2;
    m_DataAvail = 0;
  }

  void StormSocketBuffer::GotData(int bytes_received)
  {
    StormUniqueLock<StormMutex> lock(m_Mutex);

    m_WriteOffset += bytes_received;
	  m_FreeSpaceAvail.fetch_sub(bytes_received);

    while (m_WriteOffset >= m_FixedBlockSize)
    {
      m_BlockCur = m_BlockNext;
      m_BlockNext = m_Allocator->AllocateBlock(m_BlockCur, StormFixedBlockType::BlockMem);
      m_WriteOffset -= m_FixedBlockSize;
	    m_FreeSpaceAvail.fetch_add(m_FixedBlockSize);

      if (m_WriteOffset < 0)
      {
        throw std::runtime_error("Inconsistent state");
      }
    }

    if (m_WriteOffset >= m_FixedBlockSize)
    {
      throw std::runtime_error("Inconsistent state");
    }

	  m_DataAvail.fetch_add(bytes_received);

    if (m_InUse.exchange(false) == false)
    {
      throw std::runtime_error("Inconsistent state");
    }
  }

  void StormSocketBuffer::FreeBuffers()
  {
    StormUniqueLock<StormMutex> lock(m_Mutex);

    if (m_BlockStart != InvalidBlockHandle)
    {
      m_Allocator->FreeBlockChain(m_BlockStart, StormFixedBlockType::BlockMem);
    }

    m_BlockStart = InvalidBlockHandle;
    m_BlockCur = InvalidBlockHandle;
    m_BlockNext = InvalidBlockHandle;
    m_ReadOffset = 0;
    m_WriteOffset = 0;
    m_DataAvail = 0;
  }

  void StormSocketBuffer::DiscardData(int amount)
  {
    StormUniqueLock<StormMutex> lock(m_Mutex);

    if (m_DataAvail < amount)
    {
      throw std::runtime_error("Read buffer underflow");
    }

    m_ReadOffset += amount;
    while (m_ReadOffset >= m_FixedBlockSize)
    {
      m_BlockStart = m_Allocator->FreeBlock(m_BlockStart, StormFixedBlockType::BlockMem);
      m_ReadOffset -= m_FixedBlockSize;
    }

    m_DataAvail.fetch_sub(amount);
  }

  int StormSocketBuffer::GetDataAvailable() const
  {
    return m_DataAvail.load();
  }

  int StormSocketBuffer::BlockRead(void * buffer, int size)
  {
    StormUniqueLock<StormMutex> lock(m_Mutex);

	  int read = 0;

	  while (size > 0)
	  {
      if (m_DataAvail == 0)
      {
        break;
      }

		  void * block_start = m_Allocator->ResolveHandle(m_BlockStart);
		  block_start = Marshal::MemOffset(block_start, m_ReadOffset);

		  int mem_avail = m_FixedBlockSize - m_ReadOffset;
		  mem_avail = std::min(mem_avail, (int)m_DataAvail);
		  mem_avail = std::min(mem_avail, (int)size);

		  memcpy(buffer, block_start, mem_avail);
      buffer = Marshal::MemOffset(buffer, mem_avail);

      std::atomic_thread_fence(std::memory_order_release);

      m_ReadOffset += mem_avail;
      if (m_ReadOffset >= m_FixedBlockSize)
      {
        m_BlockStart = m_Allocator->FreeBlock(m_BlockStart, StormFixedBlockType::BlockMem);
        m_ReadOffset -= m_FixedBlockSize;
      }

      if (m_ReadOffset < 0)
      {
        throw std::runtime_error("Read buffer underflow");
      }

      m_DataAvail.fetch_sub(mem_avail);

		  size -= mem_avail;
      read += mem_avail;
	  }

    return read;
  }

  bool StormSocketBuffer::GetPointerInfo(StormSocketBufferWriteInfo & info)
  {
    StormUniqueLock<StormMutex> lock(m_Mutex);
    if (m_InUse.exchange(true) == true)
    {
      return false;
    }

    void * buffer_start =
      Marshal::MemOffset(m_Allocator->ResolveHandle(m_BlockCur), m_WriteOffset);

    info.m_Ptr1 = buffer_start;
    info.m_Ptr1Size = m_FixedBlockSize - m_WriteOffset;

    info.m_Ptr2 = m_Allocator->ResolveHandle(m_BlockNext);
    info.m_Ptr2Size = m_FixedBlockSize;
    return true;
  }
}
