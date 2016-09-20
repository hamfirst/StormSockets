#pragma once

#include "StormSockets\StormGenIndex.h"

namespace StormSockets
{
  namespace StormFixedBlockType
  {
    enum Index
    {
      BlockMem,
      Reader,
      Sender,
      Custom,
    };
  };

  struct StormFixedBlockHandle
  {
    int m_Index;

    StormFixedBlockHandle()
    {
    }

    StormFixedBlockHandle(int index)
    {
      m_Index = index;
    }

    bool operator == (const StormFixedBlockHandle & rhs)
    {
      return m_Index == rhs.m_Index;
    }

    bool operator != (const StormFixedBlockHandle & rhs)
    {
      return m_Index != rhs.m_Index;
    }

    operator int()
    {
      return m_Index;
    }
  };

  static const StormFixedBlockHandle InvalidBlockHandle = StormFixedBlockHandle(-1);

  struct StormFixedBlock
  {
    void * BlockPtr;
    StormFixedBlockHandle Next;
  };

  class StormFixedBlockAllocator
  {
    unsigned char * m_BlockMem;
    StormFixedBlockHandle * m_NextBlockList;
    StormGenIndex m_BlockHead;
    unsigned int m_BlockSize;
    bool m_UseVirtual;
    unsigned char * m_AllocState;

  public:

    StormFixedBlockAllocator(int total_size, int block_size, bool use_virtual);
    ~StormFixedBlockAllocator();

    int GetBlockSize() { return m_BlockSize; }

    StormFixedBlockHandle AllocateBlock(StormFixedBlockType::Index type);
    StormFixedBlockHandle AllocateBlock(StormFixedBlockHandle chain_head, StormFixedBlockType::Index type);

    StormFixedBlockHandle FreeBlock(StormFixedBlockHandle handle, StormFixedBlockType::Index type);
    StormFixedBlockHandle FreeBlock(void * resolved_pointer, StormFixedBlockType::Index type);

    void * ResolveHandle(StormFixedBlockHandle handle);

    StormFixedBlockHandle GetHandleForBlock(void * resolved_pointer);

    StormFixedBlockHandle GetNextBlock(StormFixedBlockHandle handle);
    StormFixedBlockHandle GetPrevBlock(StormFixedBlockHandle chain_start, StormFixedBlockHandle handle);

    void * GetNextBlock(void * resolved_pointer);

    void SetNextBlock(StormFixedBlockHandle handle, StormFixedBlockHandle next_block);
    void SetNextBlock(void * resolved_pointer, void * next_block_ptr);

    void FreeBlockChain(StormFixedBlockHandle handle, StormFixedBlockType::Index type);
    void FreeBlockChain(void * resolved_pointer, StormFixedBlockType::Index type);
  };
}
