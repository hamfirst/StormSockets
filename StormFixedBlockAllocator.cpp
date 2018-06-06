
#ifdef _WINDOWS
#include <Windows.h>
#endif

#include "StormFixedBlockAllocator.h"
#include "StormMemOps.h"

#include <atomic>
#include <stdexcept>

namespace StormSockets
{
  StormFixedBlockAllocator::StormFixedBlockAllocator(int total_size, int block_size, bool use_virtual)
  {
    void * block_mem;

#ifdef _WINDOWS
    //if (use_virtual)
    //{
    //  block_mem = VirtualAlloc(NULL, total_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    //}
    //else
    {
      block_mem = malloc(total_size);
    }
#else
    block_mem = malloc(total_size);
#endif

    int memory_block_size = block_size + sizeof(StormFixedBlockHandle);

    // Set up the stack - each block points to the one prior
    int num_blocks = total_size / memory_block_size;
    int * block_list = (int *)malloc(sizeof(int) * num_blocks);

    for (int block = 0; block < num_blocks; block++)
    {
      block_list[block] = block - 1;
    }

    // Init allocator members
    m_BlockMem = (unsigned char *)block_mem;
    m_NumBlocks = num_blocks;
    m_NextBlockList = block_list;
    m_BlockHead = StormGenIndex(num_blocks - 1, 0);
    m_MemoryBlockSize = memory_block_size;
    m_BlockSize = block_size;
    m_UseVirtual = use_virtual;
    m_OutstandingMallocs = 0;
  }

  StormFixedBlockAllocator::~StormFixedBlockAllocator()
  {
#ifdef _WINDOWS
    //if (m_UseVirtual)
    //{
    //  VirtualFree(m_BlockMem, 0, MEM_RELEASE);
    //}
    //else
    {
      free(m_BlockMem);
    }
#else
    free(m_BlockMem);
#endif

    free(m_NextBlockList);
  }

  void * StormFixedBlockAllocator::AllocateBlockInternal(StormFixedBlockType::Index type, StormFixedBlockHandle & handle)
  {
    while (true)
    {
      // Read the list head
      StormGenIndex list_head = m_BlockHead;
      int list_head_index = list_head.GetIndex();
      if (list_head_index == -1)
      {
        void * block_mem;
#ifdef _WINDOWS
        //if (m_UseVirtual)
        //{
        //  block_mem = VirtualAlloc(NULL, m_MemoryBlockSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        //}
        //else
        {
          block_mem = malloc(m_MemoryBlockSize);
        }
#else
        block_mem = malloc(m_MemoryBlockSize);
#endif
        if (block_mem == nullptr)
        {
          throw std::runtime_error("out of memory");
        }

        m_OutstandingMallocs++;
        handle = StormFixedBlockHandle{ -1, block_mem };
        return block_mem;
      }

      // The new head is whatever the current head is pointing to
      StormGenIndex new_head = StormGenIndex(m_NextBlockList[list_head_index], list_head.GetGen() + 1);

      // Write the new head back to memory
      if (std::atomic_compare_exchange_weak((std::atomic_uint *)&m_BlockHead.Raw, (unsigned int *)&list_head.Raw, new_head.Raw))
      {
        if (m_NextBlockList[list_head_index] == -2)
        {
          throw std::runtime_error("Invalid allocator state");
        }

        m_NextBlockList[list_head_index] = -2;
        handle = StormFixedBlockHandle{ list_head_index, nullptr };
        return &m_BlockMem[list_head_index * m_MemoryBlockSize];
      }
    }
  }

  StormFixedBlockHandle StormFixedBlockAllocator::AllocateBlock(StormFixedBlockType::Index type)
  {
    StormFixedBlockHandle handle;
    void * memory_block = AllocateBlockInternal(type, handle);
    StormFixedBlockHandle * next_block_mem = (StormFixedBlockHandle *)Marshal::MemOffset(memory_block, m_BlockSize);

    *next_block_mem = InvalidBlockHandle;
    return handle;
  }

  StormFixedBlockHandle StormFixedBlockAllocator::AllocateBlock(StormFixedBlockHandle chain_head, StormFixedBlockType::Index type)
  {
    StormFixedBlockHandle new_block = AllocateBlock(type);
    SetNextBlock(chain_head, new_block);
    return new_block;
  }

  StormFixedBlockHandle StormFixedBlockAllocator::FreeBlock(StormFixedBlockHandle handle, StormFixedBlockType::Index type)
  {
    if (handle == InvalidBlockHandle)
    {
      return InvalidBlockHandle;
    }

    StormFixedBlockHandle block_next = GetNextBlock(handle);

    if (handle.m_Index < 0)
    {
#ifdef _WINDOWS
      //if (m_UseVirtual)
      //{
      //  VirtualFree(handle.m_MallocBlock, 0, MEM_RELEASE);
      //}
      //else
      {
        free(handle.m_MallocBlock);
      }
#else
      free(handle.m_MallocBlock);
#endif

      m_OutstandingMallocs--;
      return block_next;
    }

    while (true)
    {
      // Read the list head
      StormGenIndex list_head = m_BlockHead;

      // Write out the old list head to the new head's next pointer
      m_NextBlockList[handle.m_Index] = list_head.GetIndex();

      // Swap the new value in
      StormGenIndex new_head = StormGenIndex(handle.m_Index, list_head.GetGen() + 1);
      if (std::atomic_compare_exchange_weak((std::atomic_uint *)&m_BlockHead.Raw, (unsigned int *)&list_head.Raw, new_head.Raw))
      {
        return block_next;
      }
    }
  }

  StormFixedBlockHandle StormFixedBlockAllocator::FreeBlock(void * resolved_pointer, StormFixedBlockType::Index type)
  {
    StormFixedBlockHandle handle = GetHandleForBlock(resolved_pointer);
    return FreeBlock(handle, type);
  }

  void * StormFixedBlockAllocator::ResolveHandle(StormFixedBlockHandle handle)
  {
    if (handle == InvalidBlockHandle)
    {
      return NULL;
    }

    if (handle.m_Index < 0)
    {
      return handle.m_MallocBlock;
    }

    return m_BlockMem + (handle.m_Index * m_MemoryBlockSize);
  }

  StormFixedBlockHandle StormFixedBlockAllocator::GetHandleForBlock(void * resolved_pointer)
  {
    if (resolved_pointer == NULL)
    {
      return InvalidBlockHandle;
    }

    size_t offset = (unsigned char *)resolved_pointer - (unsigned char *)m_BlockMem;
    if (offset < m_NumBlocks * m_MemoryBlockSize)
    {
      return StormFixedBlockHandle{ (int)(offset / m_MemoryBlockSize), nullptr };
    }

    return StormFixedBlockHandle{ -1, resolved_pointer };
  }

  StormFixedBlockHandle StormFixedBlockAllocator::GetNextBlock(StormFixedBlockHandle handle)
  {
    if (handle == InvalidBlockHandle)
    {
      return InvalidBlockHandle;
    }

    void * block_mem = ResolveHandle(handle);
    StormFixedBlockHandle * next_block_mem = (StormFixedBlockHandle *)Marshal::MemOffset(block_mem, m_BlockSize);
    return *next_block_mem;
  }

  StormFixedBlockHandle StormFixedBlockAllocator::GetPrevBlock(StormFixedBlockHandle chain_start, StormFixedBlockHandle handle)
  {
    if (chain_start == handle)
    {
      return InvalidBlockHandle;
    }

    StormFixedBlockHandle next = GetNextBlock(chain_start);
    while (next != InvalidBlockHandle)
    {
      if (next == handle)
      {
        return chain_start;
      }

      chain_start = next;
      next = GetNextBlock(chain_start);
    }

    return InvalidBlockHandle;
  }

  void * StormFixedBlockAllocator::GetNextBlock(void * resolved_pointer)
  {
    if (resolved_pointer == NULL)
    {
      return NULL;
    }

    return ResolveHandle(GetNextBlock(GetHandleForBlock(resolved_pointer)));
  }

  void StormFixedBlockAllocator::SetNextBlock(StormFixedBlockHandle handle, StormFixedBlockHandle next_block)
  {
    if (handle == InvalidBlockHandle)
    {
      return;
    }

    void * block_mem = ResolveHandle(handle);
    StormFixedBlockHandle * next_block_mem = (StormFixedBlockHandle *)Marshal::MemOffset(block_mem, m_BlockSize);
    *next_block_mem = next_block;
  }

  void StormFixedBlockAllocator::SetNextBlock(void * resolved_pointer, void * next_block_ptr)
  {
    StormFixedBlockHandle handle = GetHandleForBlock(resolved_pointer);
    StormFixedBlockHandle next_block = GetHandleForBlock(next_block_ptr);
    SetNextBlock(handle, next_block);
  }

  void StormFixedBlockAllocator::FreeBlockChain(StormFixedBlockHandle handle, StormFixedBlockType::Index type)
  {
    StormFixedBlockHandle cur = handle;

    while (cur != InvalidBlockHandle)
    {
      cur = FreeBlock(cur, type);
    }
  }

  void StormFixedBlockAllocator::FreeBlockChain(void * resolved_pointer, StormFixedBlockType::Index type)
  {
    FreeBlockChain(GetHandleForBlock(resolved_pointer), type);
  }
}