#include "StormWebsocketMessageReader.h"
#include "StormMemOps.h"
#include "StormProfiling.h"

#include <stdexcept>

namespace StormSockets
{
  StormWebsocketMessageReader::StormWebsocketMessageReader(StormFixedBlockAllocator * block_allocator, StormFixedBlockAllocator * reader_allocator, StormFixedBlockHandle cur_block,
    int data_len, int parse_offset, StormSocketConnectionId connection_id, int fixed_block_size)
  {
    m_Allocator = block_allocator;
    m_ReaderAllocator = reader_allocator;
    m_FixedBlockSize = fixed_block_size;

    StormFixedBlockHandle packet_handle = reader_allocator->AllocateBlock(StormFixedBlockType::Reader);
    StormMessageReaderData * packet_info = (StormMessageReaderData *)reader_allocator->ResolveHandle(packet_handle);

    m_PacketHandle = packet_handle;
    m_PacketInfo = packet_info;
    m_ConnectionId = connection_id;

    void * cur_block_ptr = m_Allocator->ResolveHandle(cur_block);
    m_PacketInfo->m_CurBlock = cur_block_ptr;
    m_PacketInfo->m_DataLength = data_len;
    m_PacketInfo->m_ReadOffset = parse_offset;
  }


	StormSocketWebsocketDataType::Index StormWebsocketMessageReader::GetDataType()
	{
		return m_DataType;
	}

	bool StormWebsocketMessageReader::GetFinalInSequence()
	{
		return m_FinalInSequence;
	}

	void StormWebsocketMessageReader::SetNextBlock(const StormWebsocketMessageReader & next_reader)
	{
		m_ReaderAllocator->SetNextBlock(m_PacketHandle, next_reader.m_PacketHandle);
	}

	bool StormWebsocketMessageReader::InvalidateNext(StormWebsocketMessageReader & next_reader)
	{
		StormMessageReaderData * next = (StormMessageReaderData *)m_ReaderAllocator->GetNextBlock(m_PacketInfo);
		if (next == NULL)
		{
			return false;
		}

		next_reader = *this;
		next_reader.m_PacketInfo = next;
		return true;
	}

	void StormWebsocketMessageReader::AddLength(int data_len)
	{
		m_FullDataLen += data_len;
	}

	void StormWebsocketMessageReader::Advance()
	{
		StormMessageReaderData * next_reader = (StormMessageReaderData *)m_ReaderAllocator->GetNextBlock(m_PacketInfo);
		if (next_reader == NULL)
		{
			throw std::runtime_error("Read buffer underflow");
		}

		if (next_reader == m_PacketInfo)
		{
			throw std::runtime_error("Inconsistent reader");
		}

		void * cur_block = next_reader->m_CurBlock;
		int read_offset = next_reader->m_ReadOffset;
		int data_length = next_reader->m_DataLength;
		StormMessageReaderData * next_next_reader = (StormMessageReaderData *)m_ReaderAllocator->GetNextBlock(next_reader);

		m_PacketInfo->m_CurBlock = cur_block;
		m_PacketInfo->m_ReadOffset = read_offset;
		m_PacketInfo->m_DataLength = data_length;
		m_ReaderAllocator->SetNextBlock(m_PacketInfo, next_next_reader);

		m_ReaderAllocator->FreeBlock(next_reader, StormFixedBlockType::Reader);
	}

	void StormWebsocketMessageReader::FreeChain()
	{
		StormFixedBlockHandle cur_packet = m_PacketHandle;

		while (cur_packet != InvalidBlockHandle)
		{
			StormFixedBlockHandle next_packet = m_ReaderAllocator->FreeBlock(cur_packet, StormFixedBlockType::Reader);
			cur_packet = next_packet;
		}
	}

	int StormWebsocketMessageReader::GetDataLength()
	{
		return m_FullDataLen;
	}

	uint8_t StormWebsocketMessageReader::ReadByte()
	{
		uint64_t prof = Profiling::StartProfiler();

		void * cur_block = m_PacketInfo->m_CurBlock;
		int read_offset = m_PacketInfo->m_ReadOffset;
		int data_length = m_PacketInfo->m_DataLength;

		if (data_length < 1)
		{
			Advance();
			Profiling::EndProfiler(prof, ProfilerCategory::kReadByte);
			return ReadByte();
		}

		uint8_t v = Marshal::ReadByte(cur_block, read_offset);

		read_offset += 1;
		data_length -= 1;

		if (read_offset >= m_FixedBlockSize)
		{
			cur_block = m_Allocator->GetNextBlock(cur_block);
			m_PacketInfo->m_CurBlock = cur_block;
			read_offset = 0;
		}

		m_PacketInfo->m_ReadOffset = read_offset;
		m_PacketInfo->m_DataLength = data_length;

		Profiling::EndProfiler(prof, ProfilerCategory::kReadByte);
		return v;
	}

	wchar_t StormWebsocketMessageReader::ReadUTF8Char()
	{
		int b1 = ReadByte();
		if (b1 < 127)
		{
			return (wchar_t)b1;
		}

		int b2 = ReadByte();
		if (b1 < 223)
		{
			return (wchar_t)(((b2 & 0x3F) | ((b1 & 0x1F) << 6)) + 0x80);
		}

		int b3 = ReadByte();
		return (wchar_t)(((b3 & 0x3F) | ((b2 & 0x3F) << 4) | ((b1 & 0x0F) << 10)) + 0x800);
	}

	uint16_t StormWebsocketMessageReader::ReadInt16()
	{
		int read_offset = m_PacketInfo->m_ReadOffset;
		int data_length = m_PacketInfo->m_DataLength;
		if (read_offset + 2 > m_FixedBlockSize || data_length < 2)
		{
			uint16_t v1 = ReadByte() & 0xFF;
			uint16_t v2 = ReadByte() & 0xFF;

			return (uint16_t)((v2 << 8) | v1);
		}

		void * cur_block = m_PacketInfo->m_CurBlock;

		short v = Marshal::ReadInt16(cur_block, read_offset);

		read_offset += 2;
		data_length -= 2;

		if (read_offset >= m_FixedBlockSize)
		{
			cur_block = m_Allocator->GetNextBlock(cur_block);
			m_PacketInfo->m_CurBlock = cur_block;
			read_offset = 0;
		}

		m_PacketInfo->m_ReadOffset = read_offset;
		m_PacketInfo->m_DataLength = data_length;

		return v;
	}

	uint32_t StormWebsocketMessageReader::ReadInt32()
	{
		int read_offset = m_PacketInfo->m_ReadOffset;
		int data_length = m_PacketInfo->m_DataLength;
		if (read_offset + 4 > m_FixedBlockSize || data_length < 4)
		{
			uint32_t v1 = ReadInt16() & 0xFFFF;
			uint32_t v2 = ReadInt16() & 0xFFFF;

			return (uint32_t)((v2 << 16) | v1);
		}

		void * cur_block = m_PacketInfo->m_CurBlock;

		uint32_t v = Marshal::ReadInt32(cur_block, read_offset);

		read_offset += 4;
		data_length -= 4;

		if (read_offset >= m_FixedBlockSize)
		{
			cur_block = m_Allocator->GetNextBlock(cur_block);
			m_PacketInfo->m_CurBlock = cur_block;
			read_offset = 0;
		}

		m_PacketInfo->m_ReadOffset = read_offset;
		m_PacketInfo->m_DataLength = data_length;

		return v;
	}

	uint64_t StormWebsocketMessageReader::ReadInt64()
	{
		int read_offset = m_PacketInfo->m_ReadOffset;
		int data_length = m_PacketInfo->m_DataLength;
		if (read_offset + 8 > m_FixedBlockSize || data_length < 8)
		{
			uint64_t v1 = ReadInt32() & 0xFFFFFFFF;
			uint64_t v2 = ReadInt32() & 0xFFFFFFFF;

			return (uint64_t)((v2 << 32) | v1);
		}

		void * cur_block = m_PacketInfo->m_CurBlock;

		uint64_t v = Marshal::ReadInt64(cur_block, read_offset);

		read_offset += 8;
		data_length -= 8;

		if (read_offset >= m_FixedBlockSize)
		{
			cur_block = m_Allocator->GetNextBlock(cur_block);
			m_PacketInfo->m_CurBlock = cur_block;
			read_offset = 0;
		}

		m_PacketInfo->m_ReadOffset = read_offset;
		m_PacketInfo->m_DataLength = data_length;
		return v;
	}

  void StormWebsocketMessageReader::ReadByteBlock(void * data, unsigned int length)
  {
    void * cur_block = m_PacketInfo->m_CurBlock;
    int read_offset = m_PacketInfo->m_ReadOffset;
    int data_length = m_PacketInfo->m_DataLength;

    while (length > 0)
    {
      while (data_length > 0)
      {
        unsigned int data_avail = m_FixedBlockSize - read_offset;

        if (data_avail > length)
        {
          memcpy(data, Marshal::MemOffset(cur_block, read_offset), length);
          read_offset += length;
          data_length -= length;
          length = 0;
          break;
        }
        else
        {
          memcpy(data, Marshal::MemOffset(cur_block, read_offset), data_avail);
          length -= data_avail;
          data_length -= data_avail;

          data = Marshal::MemOffset(data, data_avail);
          cur_block = m_Allocator->GetNextBlock(cur_block);
          read_offset = 0;
        }
      }

      if (length > 0 && data_length == 0)
      {
        Advance();

        cur_block = m_PacketInfo->m_CurBlock;
        read_offset = m_PacketInfo->m_ReadOffset;
        data_length = m_PacketInfo->m_DataLength;
      }
    }

    m_PacketInfo->m_CurBlock = cur_block;
    m_PacketInfo->m_ReadOffset = read_offset;
    m_PacketInfo->m_DataLength = data_length;
  }
}