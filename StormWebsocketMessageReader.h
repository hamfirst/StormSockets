#pragma once

#include "StormFixedBlockAllocator.h"
#include "StormSocketConnectionId.h"
#include "StormMessageReaderData.h"

#include <cstdint>


namespace StormSockets
{
	namespace StormSocketWebsocketDataType
	{
		enum Index
		{
			Binary,
			Text,
			Ping,
			Pong,
			Continuation,
		};
	}

	class StormWebsocketMessageReader
	{
		StormMessageReaderData * m_PacketInfo;
		StormFixedBlockHandle m_PacketHandle;
		StormFixedBlockAllocator * m_Allocator;
		StormFixedBlockAllocator * m_ReaderAllocator;
		int m_FixedBlockSize;
		int m_PacketDataLen;
		int m_FullDataLen;
		StormSocketConnectionId m_ConnectionId;
		StormSocketWebsocketDataType::Index m_DataType;
		bool m_FinalInSequence;

    friend class StormSocketFrontendWebsocketBase;

	public:
    StormWebsocketMessageReader() = default;
    StormWebsocketMessageReader(const StormWebsocketMessageReader & rhs) = default;

		StormSocketWebsocketDataType::Index GetDataType();

		bool GetFinalInSequence();

		int GetDataLength();

		uint8_t ReadByte();
		wchar_t ReadUTF8Char();
		uint16_t ReadInt16();
		uint32_t ReadInt32();
		uint64_t ReadInt64();

    void ReadByteBlock(void * data, unsigned int length);

  private:
    StormWebsocketMessageReader(StormFixedBlockAllocator * block_allocator, StormFixedBlockAllocator * reader_allocator, StormFixedBlockHandle cur_block,
      int data_len, int parse_offset, StormSocketConnectionId connection_id, int fixed_block_size);

    void SetNextBlock(const StormWebsocketMessageReader & next_reader);
    bool InvalidateNext(StormWebsocketMessageReader & next_reader);
    void AddLength(int data_len);

    void Advance();
    void FreeChain();
	};
}
