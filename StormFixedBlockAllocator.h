#pragma once

#include "StormGenIndex.h"

#include <atomic>

namespace StormSockets
{
  namespace StormFixedBlockType
  {
    enum Index
    {
      BlockMem,
      Reader,
      Sender,
      SendBlock,
      Custom,
    };
  };

  struct StormFixedBlockHandle
  {
    int m_Index;
    void * m_MallocBlock;

    bool operator == (const StormFixedBlockHandle & rhs)
    {
      return m_Index == rhs.m_Index && m_MallocBlock == rhs.m_MallocBlock;
    }

    bool operator != (const StormFixedBlockHandle & rhs)
    {
      return m_Index != rhs.m_Index || m_MallocBlock != rhs.m_MallocBlock;
    }
  };

  static const StormFixedBlockHandle InvalidBlockHandle = StormFixedBlockHandle{ -1, nullptr };

  struct StormFixedBlock
  {
    void * BlockPtr;
    StormFixedBlockHandle Next;
  };

  class StormFixedBlockAllocator
  {
    unsigned char * m_BlockMem;
    int * m_NextBlockList;
    StormGenIndex m_BlockHead;
    unsigned int m_NumBlocks;
    unsigned int m_BlockSize;
    unsigned int m_MemoryBlockSize;
    std::atomic_int m_OutstandingMallocs;
    bool m_UseVirtual;

  public:

    StormFixedBlockAllocator(int total_size, int block_size, bool use_virtual);
    ~StormFixedBlockAllocator();

    int GetBlockSize() { return m_BlockSize; }
    int GetOutstandingMallocs() { return m_OutstandingMallocs; }

  private:
    void * AllocateBlockInternal(StormFixedBlockType::Index type, StormFixedBlockHandle & handle);

  public:
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
