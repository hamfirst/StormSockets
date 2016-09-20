#include "StormHttpRequestReader.h"
#include "StormMemOps.h"
#include "StormProfiling.h"

#include <stdexcept>

namespace StormSockets
{
  StormHttpRequestReader::StormHttpRequestReader(void * block, int data_len, int read_offset, StormSocketConnectionId connection_id,
    StormFixedBlockAllocator * block_allocator, StormFixedBlockAllocator * reader_allocator,
    const StormMessageReaderCursor & method, StormMessageReaderCursor & uri, StormMessageHeaderReader & headers) :
    m_Method(method),
    m_URI(uri),
    m_Headers(headers)
  {
    m_Allocator = block_allocator;
    m_ReaderAllocator = reader_allocator;

    StormFixedBlockHandle packet_handle = m_ReaderAllocator->AllocateBlock(StormFixedBlockType::Reader);
    StormMessageReaderData * packet_info = (StormMessageReaderData *)m_ReaderAllocator->ResolveHandle(packet_handle);

    m_FullDataLen = 0;
    m_BodyDataLen = data_len;

    m_FirstPacketHandle = packet_handle;
    m_LastPacketHandle = packet_handle;
    m_ConnectionId = connection_id;

    packet_info->m_CurBlock = block;
    packet_info->m_DataLength = data_len;
    packet_info->m_ReadOffset = read_offset;
  }

  StormHttpBodyReader StormHttpRequestReader::GetBodyReader()
  {
    StormMessageReaderData * packet_info = (StormMessageReaderData *)m_ReaderAllocator->ResolveHandle(m_FirstPacketHandle);
    return StormHttpBodyReader(packet_info, m_Allocator, m_ReaderAllocator, m_BodyDataLen);
  }

  void StormHttpRequestReader::AddBlock(void * block, int data_len, int read_offset)
  {
    StormFixedBlockHandle packet_handle = m_ReaderAllocator->AllocateBlock(StormFixedBlockType::Reader);
    StormMessageReaderData * packet_info = (StormMessageReaderData *)m_ReaderAllocator->ResolveHandle(packet_handle);
    packet_info->m_CurBlock = block;
    packet_info->m_DataLength = data_len;
    packet_info->m_ReadOffset = read_offset;

    m_ReaderAllocator->SetNextBlock(m_LastPacketHandle, packet_handle);
    m_LastPacketHandle = packet_handle;

    m_BodyDataLen += data_len;
  }

  void StormHttpRequestReader::FreeChain()
  {
    StormFixedBlockHandle cur_packet = m_FirstPacketHandle;

    while (cur_packet != InvalidBlockHandle)
    {
      StormFixedBlockHandle next_packet = m_ReaderAllocator->FreeBlock(cur_packet, StormFixedBlockType::Reader);
      cur_packet = next_packet;
    }
  }
}