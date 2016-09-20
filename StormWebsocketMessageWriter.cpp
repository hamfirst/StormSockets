
#include "StormWebsocketMessageWriter.h"
#include "StormMemOps.h"


namespace StormSockets
{
  StormWebsocketMessageWriter::StormWebsocketMessageWriter(const StormMessageWriter & writer) :
    StormMessageWriter(writer)
  {

  }

  void StormWebsocketMessageWriter::SaveHeaderRoom()
  {
    // Save enough space for the header
    // In the worse case, the header is
    // 2 byte for opcode / length header
    // 8 bytes for the length
    // 4 bytes for the mask
    m_PacketInfo->m_WriteOffset = WebsocketMaxHeaderSize + m_ReservedHeaderLength;
  }

  void StormWebsocketMessageWriter::CreateHeaderAndApplyMask(int opcode, bool fin, int mask)
  {
    StormFixedBlockHandle start_handle = m_PacketInfo->m_StartBlock;
    void * start_block = m_Allocator->ResolveHandle(start_handle);

    int packet_length = m_PacketInfo->m_TotalLength;
    int base_offset;
    int payload_bits;

    //if(packet_length == 0)
    {
      mask = 0;
    }

    if (packet_length < 126)
    {
      base_offset = 8;
      payload_bits = packet_length << 8;
    }
    else if (packet_length < 65536)
    {
      base_offset = 6;
      payload_bits = 126 << 8;
    }
    else
    {
      base_offset = 0;
      payload_bits = 127 << 8;
    }

    if (mask == 0)
    {
      base_offset += 4;
    }
    else
    {
      // Enable mask
      payload_bits |= 0x8000;
    }

    m_PacketInfo->m_SendOffset = base_offset;

    // Set the op code
    payload_bits |= opcode & 0x0F;

    // Set the fin bit
    if (fin)
    {
      payload_bits |= 0x80;
    }

    int cur_offset = base_offset + m_ReservedHeaderLength;
    Marshal::WriteInt16(start_block, cur_offset, (short)payload_bits);
    cur_offset += 2;

    if (packet_length >= 126)
    {
      if (packet_length < 65536)
      {
        Marshal::WriteInt16(start_block, cur_offset, Marshal::HostToNetworkOrder((uint16_t)packet_length));
        cur_offset += 2;
      }
      else
      {
        uint64_t pl = packet_length;
        uint64_t hpl = Marshal::HostToNetworkOrder(pl);
        Marshal::WriteInt64(start_block, cur_offset, hpl);
        cur_offset += 8;
      }
    }

    if (mask != 0)
    {
      // Write the mask
      Marshal::WriteInt32(start_block, cur_offset, mask);
      cur_offset += 4;
    }

    void * cur_block = start_block;
    int read_offset = cur_offset;
    int data_length = packet_length;

    // Include the header in the total length
    int header_size = WebsocketMaxHeaderSize - base_offset;
    m_PacketInfo->m_TotalLength = data_length + header_size;

    if (mask != 0)
    {
      // Mask out the rest of the data
      do
      {
        if (data_length >= 4 && read_offset + 4 <= m_Allocator->GetBlockSize() - m_ReservedTrailerLength)
        {
          int val = Marshal::ReadInt32(cur_block, read_offset);
          val ^= mask;
          Marshal::WriteInt32(cur_block, read_offset, val);
          read_offset += 4;
          data_length -= 4;
        }
        else if (data_length > 0 && read_offset != m_Allocator->GetBlockSize() - m_ReservedTrailerLength)
        {
          int val = Marshal::ReadByte(cur_block, read_offset);
          val ^= mask;
          Marshal::WriteByte(cur_block, read_offset, (uint8_t)val);
          read_offset += 1;
          data_length -= 1;

          mask = (mask << 24) | (mask >> 8);
        }

        if (read_offset == m_Allocator->GetBlockSize() - m_ReservedTrailerLength)
        {
          cur_block = m_Allocator->GetNextBlock(cur_block);
          read_offset = m_ReservedHeaderLength;
        }
      } while (data_length > 0 && cur_block != NULL);
    }
  }
}

