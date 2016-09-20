
#ifdef _WINDOWS
#include <Windows.h>
#endif

#include "StormFixedBlockAllocator.h"

#include <atomic>
#include <stdexcept>

namespace StormSockets
{
  StormFixedBlockAllocator::StormFixedBlockAllocator(int total_size, int block_size, bool use_virtual)
  {
    void * block_mem;

#ifdef _WINDOWS
    if (use_virtual)
    {
      block_mem = VirtualAlloc(NULL, total_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    }
    else
    {
      block_mem = malloc(total_size);
    }
#else
    block_mem = malloc(total_size);
#endif

    // Set up the stack - each block points to the one prior
    int num_blocks = total_size / block_size;
    StormFixedBlockHandle * block_list = (StormFixedBlockHandle *)malloc(sizeof(StormFixedBlockHandle) * num_blocks);
    m_AllocState = (unsigned char *)malloc(num_blocks);
    for (int block = 0; block < num_blocks; block++)
    {
      block_list[block] = block - 1;
      m_AllocState[block] = 0;
    }

    // Init allocator members
    m_BlockMem = (unsigned char *)block_mem;
    m_NextBlockList = block_list;
    m_BlockHead = StormGenIndex(num_blocks - 1, 0);
    m_BlockSize = block_size;
    m_UseVirtual = use_virtual;

  }

  StormFixedBlockAllocator::~StormFixedBlockAllocator()
  {
#ifdef _WINDOWS
    if (m_UseVirtual)
    {
      VirtualFree(m_BlockMem, 0, MEM_RELEASE);
    }
    else
    {
      free(m_BlockMem);
    }
#else
    free(m_BlockMem);
#endif

    free(m_NextBlockList);
  }

  StormFixedBlockHandle StormFixedBlockAllocator::AllocateBlock(StormFixedBlockType::Index type)
  {
    while (true)
    {
      // Read the list head
      StormGenIndex list_head = m_BlockHead;
      int list_head_index = list_head.GetIndex();
      if (list_head_index == -1)
      {
        return InvalidBlockHandle;
      }

      // The new head is whatever the current head is pointing to
      StormGenIndex new_head = StormGenIndex(m_NextBlockList[list_head_index], list_head.GetGen() + 1);

      // Write the new head back to memory
      if (std::atomic_compare_exchange_weak((std::atomic_uint *)&m_BlockHead.Raw, (unsigned int *)&list_head.Raw, new_head.Raw))
      {
        if (m_AllocState[list_head_index] != 0)
        {
          throw std::runtime_error("Invalid allocator state");
        }

        m_AllocState[list_head_index] = 1;

        m_NextBlockList[list_head_index] = InvalidBlockHandle;
        return list_head_index;
      }
    }
  }

  StormFixedBlockHandle StormFixedBlockAllocator::AllocateBlock(StormFixedBlockHandle chain_head, StormFixedBlockType::Index type)
  {
    StormFixedBlockHandle list_head_index = AllocateBlock(type);

    // Copy the new block into the old head's next pointer
    m_NextBlockList[chain_head] = list_head_index;
    return list_head_index;
  }

  StormFixedBlockHandle StormFixedBlockAllocator::FreeBlock(StormFixedBlockHandle handle, StormFixedBlockType::Index type)
  {
    if (handle == InvalidBlockHandle)
    {
      return InvalidBlockHandle;
    }

    StormFixedBlockHandle block_next = m_NextBlockList[handle];

    if (m_AllocState[handle] != 1)
    {
      throw std::runtime_error("Invalid allocator state");
    }

    m_AllocState[handle] = 0;

    while (true)
    {
      // Read the list head
      StormGenIndex list_head = m_BlockHead;

      // Write out the old list head to the new head's next pointer
      m_NextBlockList[handle] = list_head.GetIndex();

      // Swap the new value in
      StormGenIndex new_head = StormGenIndex(handle, list_head.GetGen() + 1);
      if (std::atomic_compare_exchange_weak((std::atomic_uint *)&m_BlockHead.Raw, (unsigned int *)&list_head.Raw, new_head.Raw))
      {
        return block_next;
      }
    }
  }

  StormFixedBlockHandle StormFixedBlockAllocator::FreeBlock(void * resolved_pointer, StormFixedBlockType::Index type)
  {
    size_t offset = (unsigned char *)resolved_pointer - (unsigned char *)m_BlockMem;
    return FreeBlock(offset / m_BlockSize, type);
  }

  void * StormFixedBlockAllocator::ResolveHandle(StormFixedBlockHandle handle)
  {
    if (handle == InvalidBlockHandle)
    {
      return NULL;
    }

    return m_BlockMem + (handle.m_Index * m_BlockSize);
  }

  StormFixedBlockHandle StormFixedBlockAllocator::GetHandleForBlock(void * resolved_pointer)
  {
    if (resolved_pointer == NULL)
    {
      return InvalidBlockHandle;
    }

    size_t offset = (unsigned char *)resolved_pointer - (unsigned char *)m_BlockMem;
    return offset / m_BlockSize;
  }

  StormFixedBlockHandle StormFixedBlockAllocator::GetNextBlock(StormFixedBlockHandle handle)
  {
    if (handle == InvalidBlockHandle)
    {
      return InvalidBlockHandle;
    }

    return m_NextBlockList[handle];
  }

  StormFixedBlockHandle StormFixedBlockAllocator::GetPrevBlock(StormFixedBlockHandle chain_start, StormFixedBlockHandle handle)
  {
    if (chain_start == handle)
    {
      return InvalidBlockHandle;
    }


    StormFixedBlockHandle next = m_NextBlockList[chain_start];
    while (next != InvalidBlockHandle)
    {
      if (next == handle)
      {
        return chain_start;
      }

      chain_start = next;
      next = m_NextBlockList[chain_start];
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

    if (handle == next_block)
    {
      throw std::runtime_error("Invalid allocator state");
    }

    m_NextBlockList[handle] = next_block;
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