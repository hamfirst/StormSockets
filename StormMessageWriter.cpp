
#include "StormMessageWriter.h"
#include "StormMemOps.h"
#include "StormProfiling.h"

#include <stdexcept>


namespace StormSockets
{
  void StormMessageWriter::Init(StormFixedBlockAllocator * block_allocator, StormFixedBlockAllocator * sender_allocator, bool encrypted, int header_length, int trailer_length)
  {
    m_Allocator = block_allocator;
    m_SenderAllocator = sender_allocator;
    m_ReservedHeaderLength = header_length;
    m_ReservedTrailerLength = trailer_length;
    m_HeaderLength = header_length;
    m_TrailerLength = trailer_length;
    m_IsEncrypted = encrypted;

    StormFixedBlockHandle packet_handle = sender_allocator->AllocateBlock(StormFixedBlockType::Sender);
    StormMessageWriterData * packet_info = (StormMessageWriterData *)sender_allocator->ResolveHandle(packet_handle);

    m_PacketHandle = packet_handle;
    m_PacketInfo = packet_info;

    StormFixedBlockHandle block_handle = m_Allocator->AllocateBlock(StormFixedBlockType::BlockMem);

    m_PacketInfo->m_StartBlock = block_handle;
    m_PacketInfo->m_CurBlock = block_handle;
    m_PacketInfo->m_PrevBlock = InvalidBlockHandle;
    m_PacketInfo->m_WriteOffset = header_length;
    m_PacketInfo->m_TotalLength = 0;
    m_PacketInfo->m_SendOffset = 0;
    m_PacketInfo->m_RefCount = 1;
  }


  int StormMessageWriter::GetLength()
  {
    return m_PacketInfo->m_TotalLength;
  }

  void StormMessageWriter::DebugPrint()
  {
    StormFixedBlockHandle block_handle = m_PacketInfo->m_StartBlock;
    int len = m_PacketInfo->m_TotalLength;

    while (len > 0)
    {
      int block_len = m_Allocator->GetBlockSize();
      char * ptr = (char *)m_Allocator->ResolveHandle(block_handle);

      while (len > 0 && block_len > 0)
      {
        putc(*ptr, stdout);
        ptr++;

        len--;
        block_len--;
      }
      
      block_handle = m_Allocator->GetNextBlock(block_handle);
    }
  }

  void StormMessageWriter::WriteByte(uint8_t b)
  {
    uint64_t prof = Profiling::StartProfiler();

    StormFixedBlockHandle cur_block = m_PacketInfo->m_CurBlock;
    void * ptr = m_Allocator->ResolveHandle(cur_block);

    int write_offset = m_PacketInfo->m_WriteOffset;
    Marshal::WriteByte(ptr, write_offset, b);

    write_offset += 1;
    m_PacketInfo->m_TotalLength += 1;

    if (write_offset >= m_Allocator->GetBlockSize() - m_ReservedTrailerLength)
    {
      // Allocate a new block
      m_PacketInfo->m_PrevBlock = m_PacketInfo->m_CurBlock;
      m_PacketInfo->m_CurBlock = m_Allocator->AllocateBlock(cur_block, StormFixedBlockType::BlockMem);
      m_PacketInfo->m_WriteOffset = m_ReservedHeaderLength;
    }
    else
    {
      m_PacketInfo->m_WriteOffset = write_offset;
    }

    Profiling::EndProfiler(prof, ProfilerCategory::kWriteByte);
  }

  void StormMessageWriter::WriteUTF8Char(wchar_t c)
  {
    if (c < 0x80)
    {
      WriteByte((uint8_t)c);
      return;
    }

    if (c < 0x800)
    {
      c -= (wchar_t)0x80;
      uint8_t b1 = (uint8_t)((c >> 6) | 0xC0);
      uint8_t b2 = (uint8_t)((c & 0x3F) | 0x80);

      WriteByte(b1);
      WriteByte(b2);
      return;
    }

    c -= (wchar_t)0x880;
    uint8_t ba = (uint8_t)((c >> 12) | 0xE0);
    uint8_t bb = (uint8_t)(((c >> 6) & 0x3F) | 0x80);
    uint8_t bc = (uint8_t)((c & 0x3F) | 0x80);

    WriteByte(ba);
    WriteByte(bb);
    WriteByte(bc);
  }

  void StormMessageWriter::WriteInt16(uint16_t s)
  {
    int write_offset = m_PacketInfo->m_WriteOffset;
    if (write_offset + 2 > m_Allocator->GetBlockSize() - m_ReservedTrailerLength)
    {
      WriteByte((uint8_t)s);
      WriteByte((uint8_t)(s >> 8));
      return;
    }

    StormFixedBlockHandle cur_block = m_PacketInfo->m_CurBlock;
    void * ptr = m_Allocator->ResolveHandle(cur_block);
    Marshal::WriteInt16(ptr, write_offset, s);

    write_offset += 2;
    m_PacketInfo->m_TotalLength += 2;

    if (write_offset >= m_Allocator->GetBlockSize() - m_ReservedTrailerLength)
    {
      // Allocate a new block
      m_PacketInfo->m_PrevBlock = m_PacketInfo->m_CurBlock;
      m_PacketInfo->m_CurBlock = m_Allocator->AllocateBlock(cur_block, StormFixedBlockType::BlockMem);
      m_PacketInfo->m_WriteOffset = m_ReservedHeaderLength;
    }
    else
    {
      m_PacketInfo->m_WriteOffset = write_offset;
    }
  }

  void StormMessageWriter::WriteInt32(uint32_t i)
  {
    int write_offset = m_PacketInfo->m_WriteOffset;
    if (write_offset + 4 > m_Allocator->GetBlockSize() - m_ReservedTrailerLength)
    {
      WriteInt16((uint16_t)i);
      WriteInt16((uint16_t)(i >> 16));
      return;
    }

    StormFixedBlockHandle cur_block = m_PacketInfo->m_CurBlock;
    void * ptr = m_Allocator->ResolveHandle(cur_block);
    Marshal::WriteInt32(ptr, write_offset, i);

    write_offset += 4;
    m_PacketInfo->m_TotalLength += 4;

    if (write_offset >= m_Allocator->GetBlockSize() - m_ReservedTrailerLength)
    {
      // Allocate a new block
      m_PacketInfo->m_PrevBlock = m_PacketInfo->m_CurBlock;
      m_PacketInfo->m_CurBlock = m_Allocator->AllocateBlock(cur_block, StormFixedBlockType::BlockMem);
      m_PacketInfo->m_WriteOffset = m_ReservedHeaderLength;
    }
    else
    {
      m_PacketInfo->m_WriteOffset = write_offset;
    }
  }

  void StormMessageWriter::WriteInt64(uint64_t l)
  {
    int write_offset = m_PacketInfo->m_WriteOffset;
    if (write_offset + 8 > m_Allocator->GetBlockSize() - m_ReservedTrailerLength)
    {
      WriteInt32((uint32_t)l);
      WriteInt32((uint32_t)(l >> 32));
      return;
    }

    StormFixedBlockHandle cur_block = m_PacketInfo->m_CurBlock;
    void * ptr = m_Allocator->ResolveHandle(cur_block);
    Marshal::WriteInt64(ptr, write_offset, l);

    write_offset += 8;
    m_PacketInfo->m_TotalLength += 8;

    if (write_offset >= m_Allocator->GetBlockSize() - m_ReservedTrailerLength)
    {
      // Allocate a new block
      m_PacketInfo->m_PrevBlock = m_PacketInfo->m_CurBlock;
      m_PacketInfo->m_CurBlock = m_Allocator->AllocateBlock(cur_block, StormFixedBlockType::BlockMem);
      m_PacketInfo->m_WriteOffset = m_ReservedHeaderLength;
    }
    else
    {
      m_PacketInfo->m_WriteOffset = write_offset;
    }
  }

  void StormMessageWriter::WriteByteBlock(const void * buffer, int start_offset, std::size_t length)
  {
    int write_offset = m_PacketInfo->m_WriteOffset;
    StormFixedBlockHandle cur_block = m_PacketInfo->m_CurBlock;
    void * ptr = m_Allocator->ResolveHandle(cur_block);
    ptr = Marshal::MemOffset(ptr, write_offset);
    std::size_t init_length = (int)length;

    while (length > 0)
    {
      int space_avail = m_Allocator->GetBlockSize() - m_ReservedTrailerLength - write_offset;

      int write_len = (space_avail < (int)length ? space_avail : (int)length);
      Marshal::Copy(ptr, start_offset, buffer, write_len);

      length -= write_len;
      space_avail -= write_len;
      write_offset += write_len;
      buffer = Marshal::MemOffset(buffer, write_len);

      if (space_avail == 0)
      {
        // Allocate a new block
        cur_block = m_Allocator->AllocateBlock(cur_block, StormFixedBlockType::BlockMem);
        ptr = m_Allocator->ResolveHandle(cur_block);

        m_PacketInfo->m_PrevBlock = m_PacketInfo->m_CurBlock;
        m_PacketInfo->m_CurBlock = cur_block;
        m_PacketInfo->m_WriteOffset = m_ReservedHeaderLength;

        write_offset = m_ReservedHeaderLength;
      }
    }

    m_PacketInfo->m_TotalLength += (int)init_length;
    m_PacketInfo->m_WriteOffset = write_offset;
  }

  void StormMessageWriter::RemoveBytes(int length)
  {
    if (m_PacketInfo->m_TotalLength > length)
    {
      throw std::runtime_error("StormMessageWriter removing too many bytes");
    }

    for (int data_in_block = m_PacketInfo->m_WriteOffset - m_ReservedHeaderLength; data_in_block > length; data_in_block = m_PacketInfo->m_WriteOffset - m_ReservedHeaderLength)
    {
      StormFixedBlockHandle prev_block = m_PacketInfo->m_PrevBlock;
      if (prev_block == InvalidBlockHandle)
      {
        prev_block = m_Allocator->GetPrevBlock(m_PacketInfo->m_StartBlock, m_PacketInfo->m_CurBlock);
      }

      // Free the last block
      StormFixedBlockHandle cur_block = m_PacketInfo->m_CurBlock;
      m_Allocator->FreeBlock(cur_block, StormFixedBlockType::BlockMem);
      m_Allocator->SetNextBlock(prev_block, InvalidBlockHandle);

      length -= data_in_block;
      m_PacketInfo->m_TotalLength -= data_in_block;
      m_PacketInfo->m_WriteOffset = m_Allocator->GetBlockSize() - m_ReservedTrailerLength;
    }

    m_PacketInfo->m_WriteOffset -= length;
    m_PacketInfo->m_TotalLength -= length;
  }

  void StormMessageWriter::WriteString(const char * str)
  {
    WriteByteBlock(str, 0, strlen(str));
  }

  uint8_t * StormMessageWriter::GetCurrentWriteAddress()
  {
    StormFixedBlockHandle cur_block = m_PacketInfo->m_CurBlock;
    void * ptr = m_Allocator->ResolveHandle(cur_block);

    return (uint8_t *)Marshal::MemOffset(ptr, m_PacketInfo->m_WriteOffset);
  }

  void StormMessageWriter::AdvanceByte()
  {
    uint64_t prof = Profiling::StartProfiler();

    StormFixedBlockHandle cur_block = m_PacketInfo->m_CurBlock;
    void * ptr = m_Allocator->ResolveHandle(cur_block);

    int write_offset = m_PacketInfo->m_WriteOffset;

    write_offset += 1;
    m_PacketInfo->m_TotalLength += 1;

    if (write_offset >= m_Allocator->GetBlockSize() - m_ReservedTrailerLength)
    {
      // Allocate a new block
      m_PacketInfo->m_PrevBlock = m_PacketInfo->m_CurBlock;
      m_PacketInfo->m_CurBlock = m_Allocator->AllocateBlock(cur_block, StormFixedBlockType::BlockMem);
      m_PacketInfo->m_WriteOffset = m_ReservedHeaderLength;
    }
    else
    {
      m_PacketInfo->m_WriteOffset = write_offset;
    }

    Profiling::EndProfiler(prof, ProfilerCategory::kWriteByte);
  }
}

