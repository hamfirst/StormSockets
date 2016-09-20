
#include "StormHttpBodyReader.h"
#include "StormProfiling.h"
#include "StormMemOps.h"

#include <stdexcept>

namespace StormSockets
{
  StormHttpBodyReader::StormHttpBodyReader(StormMessageReaderData * packet_info, StormFixedBlockAllocator * allocator, StormFixedBlockAllocator * reader_allocator, int full_data_len) :
    m_Reader(allocator, packet_info->m_CurBlock, packet_info->m_DataLength, packet_info->m_ReadOffset)
  {
    m_ReaderAllocator = reader_allocator;
    m_PacketInfo = packet_info;
    m_FullDataLen = full_data_len;
  }

  void StormHttpBodyReader::Advance()
  {
    StormMessageReaderData * next_reader = (StormMessageReaderData *)m_ReaderAllocator->GetNextBlock(m_PacketInfo);
    if (next_reader == nullptr)
    {
      throw std::runtime_error("Read buffer underflow");
    }

    if (next_reader == m_PacketInfo)
    {
      throw std::runtime_error("Inconsistent reader");
    }

    m_Reader = StormMessageReaderCursor(m_Reader.m_Allocator, next_reader->m_CurBlock, next_reader->m_DataLength, next_reader->m_ReadOffset);
    m_PacketInfo = next_reader;
  }

  uint8_t StormHttpBodyReader::ReadByte()
  {
    uint64_t prof = Profiling::StartProfiler();

    if (m_Reader.GetRemainingLength() < 1)
    {
      Advance();
      Profiling::EndProfiler(prof, ProfilerCategory::kReadByte);
      return ReadByte();
    }

    auto v = m_Reader.ReadByte();
    Profiling::EndProfiler(prof, ProfilerCategory::kReadByte);
    return v;
  }

  wchar_t StormHttpBodyReader::ReadUTF8Char()
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

  uint16_t StormHttpBodyReader::ReadInt16()
  {
    if (m_Reader.GetRemainingLength() < 2)
    {
      uint16_t v1 = ReadByte() & 0xFF;
      uint16_t v2 = ReadByte() & 0xFF;

      return (uint16_t)((v2 << 8) | v1);
    }

    return m_Reader.ReadInt16();
  }

  uint32_t StormHttpBodyReader::ReadInt32()
  {
    if (m_Reader.GetRemainingLength() < 4)
    {
      uint32_t v1 = ReadInt16() & 0xFFFF;
      uint32_t v2 = ReadInt16() & 0xFFFF;

      return (uint32_t)((v2 << 16) | v1);
    }

    return m_Reader.ReadInt32();
  }

  uint64_t StormHttpBodyReader::ReadInt64()
  {
    if (m_Reader.GetRemainingLength() < 8)
    {
      uint64_t v1 = ReadInt32() & 0xFFFFFFFF;
      uint64_t v2 = ReadInt32() & 0xFFFFFFFF;

      return (uint64_t)((v2 << 32) | v1);
    }

    return m_Reader.ReadInt64();
  }

  void StormHttpBodyReader::ReadByteBlock(void * buffer, int length)
  {
    while (length > m_Reader.GetRemainingLength())
    {
      int copy_len = m_Reader.GetRemainingLength();
      m_Reader.ReadByteBlock(buffer, copy_len);

      Advance();

      buffer = Marshal::MemOffset(buffer, copy_len);
      length -= copy_len;
    }

    m_Reader.ReadByteBlock(buffer, length);
  }
}

