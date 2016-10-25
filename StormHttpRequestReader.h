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
  class StormHttpRequestReader
  {
    StormFixedBlockHandle m_FirstPacketHandle;
    StormFixedBlockHandle m_LastPacketHandle;
    StormFixedBlockAllocator * m_Allocator;
    StormFixedBlockAllocator * m_ReaderAllocator;

    int m_FullDataLen;
    int m_BodyDataLen;
    StormSocketConnectionId m_ConnectionId;

    StormMessageReaderCursor m_Method;
    StormMessageReaderCursor m_URI;
    StormMessageHeaderReader m_Headers;

    friend class StormSocketServerFrontendHttp;

  public:
    StormHttpRequestReader() = default;
    StormHttpRequestReader(const StormHttpRequestReader & rhs) = default;

    StormHttpBodyReader GetBodyReader();

    StormMessageReaderCursor & GetMethod() { return m_Method; };
    StormMessageReaderCursor & GetURI() { return m_URI; }
    StormMessageHeaderReader & GetHeaderReader() { return m_Headers; }

  private:
    StormHttpRequestReader(void * block, int data_len, int read_offset, StormSocketConnectionId connection_id,
      StormFixedBlockAllocator * block_allocator, StormFixedBlockAllocator * reader_allocator,
      const StormMessageReaderCursor & method, StormMessageReaderCursor & uri, StormMessageHeaderReader & headers);

    void AddBlock(void * block, int data_len, int read_offset);
    void FreeChain();
    void FinalizeFullDataLength(int size) { m_FullDataLen = size; }
  };
}
