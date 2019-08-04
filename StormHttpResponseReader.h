#pragma once

#include "StormFixedBlockAllocator.h"
#include "StormSocketConnectionId.h"
#include "StormMessageReaderData.h"
#include "StormMessageReaderCursor.h"
#include "StormMessageHeaderReader.h"
#include "StormHttpBodyReader.h"

#include <cstdint>


namespace StormSockets
{

	class StormHttpResponseReader
	{
    StormFixedBlockHandle m_FirstPacketHandle;
    StormFixedBlockHandle m_LastPacketHandle;
    StormFixedBlockAllocator * m_Allocator;
    StormFixedBlockAllocator * m_ReaderAllocator;

		int m_FullDataLen;
    int m_BodyDataLen;
		StormSocketConnectionId m_ConnectionId;

    StormMessageReaderCursor m_StatusLine;
    StormMessageReaderCursor m_ResponsePhrase;
    StormMessageHeaderReader m_Headers;
    int m_ResponseCode;

    friend class StormSocketClientFrontendHttp;
    friend class StormSocketFrontendHttpBase;

	public:
    StormHttpResponseReader() = default;
    StormHttpResponseReader(const StormHttpResponseReader & rhs) = default;

    StormHttpBodyReader GetBodyReader();

    StormMessageReaderCursor GetStatusLineReader() { return m_StatusLine; };
    StormMessageReaderCursor GetResponsePhraseReader() { return m_ResponsePhrase; }
    StormMessageHeaderReader GetHeaderReader() { return m_Headers; }
    int GetResponseCode() const { return m_ResponseCode; }

  private:
    StormHttpResponseReader(void * block, int data_len, int read_offset, StormSocketConnectionId connection_id,
      StormFixedBlockAllocator * block_allocator, StormFixedBlockAllocator * reader_allocator,
      const StormMessageReaderCursor & status_line, StormMessageReaderCursor & response_phrase, StormMessageHeaderReader & headers,
      int response_code);

    void AddBlock(void * block, int data_len, int read_offset);
    void FreeChain();
    void FinalizeFullDataLength(int size) { m_FullDataLen = size; }
	};
}
