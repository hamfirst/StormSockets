
#include "StormSocketBuffer.h"

#include <stdexcept>

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
    return *this;
  }

  void StormSocketBuffer::InitBuffers()
  {
    m_BlockStart = m_Allocator->AllocateBlock(StormFixedBlockType::BlockMem);
    m_BlockCur = m_BlockStart;
    m_BlockNext = m_Allocator->AllocateBlock(m_BlockCur, StormFixedBlockType::BlockMem);
  }

  void StormSocketBuffer::GotData(int bytes_received)
  {
    m_DataAvail.fetch_add(bytes_received);
    m_WriteOffset += bytes_received;

    while (m_WriteOffset >= m_FixedBlockSize)
    {
      m_BlockCur = m_BlockNext;
      m_BlockNext = m_Allocator->AllocateBlock(m_BlockCur, StormFixedBlockType::BlockMem);
      m_WriteOffset -= m_FixedBlockSize;
    }
  }

  void StormSocketBuffer::FreeBuffers()
  {
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
}