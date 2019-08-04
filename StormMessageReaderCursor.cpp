
#include "StormMessageReaderCursor.h"
#include "StormMemOps.h"

#include <stdexcept>

#include <hash/Hash.h>

namespace StormSockets
{
  StormMessageReaderCursor::StormMessageReaderCursor(StormFixedBlockAllocator * allocator, void * cur_block, int data_length, int read_offset) :
    m_Allocator(allocator),
    m_CurBlock(cur_block),
    m_DataLength(data_length),
    m_ReadOffset(read_offset),
    m_FixedBlockSize(allocator->GetBlockSize())
  {
    if (read_offset >= m_FixedBlockSize)
    {
      throw std::runtime_error("bad read offset");
    }
  }

  StormMessageReaderCursor::StormMessageReaderCursor(const StormMessageReaderCursor & rhs, int length)
    : StormMessageReaderCursor(rhs)
  {
    m_DataLength = std::min(m_DataLength, length);
  }

  uint8_t StormMessageReaderCursor::PeekByte()
  {
    if (m_DataLength < 1)
    {
      throw std::runtime_error("Read buffer underflow");
    }

    uint8_t v = Marshal::ReadByte(m_CurBlock, m_ReadOffset);
    return v;
  }

  uint8_t StormMessageReaderCursor::ReadByte()
  {
    if (m_DataLength < 1)
    {
      throw std::runtime_error("Read buffer underflow");
    }

    uint8_t v = Marshal::ReadByte(m_CurBlock, m_ReadOffset);

    m_ReadOffset += 1;
    m_DataLength -= 1;

    if (m_ReadOffset >= m_FixedBlockSize)
    {
      m_CurBlock = m_Allocator->GetNextBlock(m_CurBlock);
      m_ReadOffset = 0;
    }

    return v;
  }

  uint16_t StormMessageReaderCursor::ReadInt16()
  {
    if (m_DataLength < 2)
    {
      throw std::runtime_error("Read buffer underflow");
    }

    if (m_ReadOffset + 2 > m_FixedBlockSize)
    {
      uint16_t v1 = ReadByte();
      uint16_t v2 = ReadByte();

      return (uint16_t)((v2 << 8) | v1);
    }

    uint16_t v = Marshal::ReadInt16(m_CurBlock, m_ReadOffset);

    m_ReadOffset += 2;
    m_DataLength -= 2;

    if (m_ReadOffset >= m_FixedBlockSize)
    {
      m_CurBlock = m_Allocator->GetNextBlock(m_CurBlock);
      m_ReadOffset = 0;
    }

    return v;
  }

  uint32_t StormMessageReaderCursor::ReadInt32()
  {
    if (m_DataLength < 4)
    {
      throw std::runtime_error("Read buffer underflow");
    }

    if (m_ReadOffset + 4 > m_FixedBlockSize)
    {
      uint32_t v1 = ReadInt16();
      uint32_t v2 = ReadInt16();

      return (uint32_t)((v2 << 16) | v1);
    }

    uint32_t v = Marshal::ReadInt32(m_CurBlock, m_ReadOffset);

    m_ReadOffset += 4;
    m_DataLength -= 4;

    if (m_ReadOffset >= m_FixedBlockSize)
    {
      m_CurBlock = m_Allocator->GetNextBlock(m_CurBlock);
      m_ReadOffset = 0;
    }

    return v;
  }


  uint64_t StormMessageReaderCursor::ReadInt64()
  {
    if (m_DataLength < 8)
    {
      throw std::runtime_error("Read buffer underflow");
    }

    if (m_ReadOffset + 8 > m_FixedBlockSize)
    {
      uint64_t v1 = ReadInt32();
      uint64_t v2 = ReadInt32();

      return (uint64_t)((v2 << 32) | v1);
    }

    uint64_t v = Marshal::ReadInt64(m_CurBlock, m_ReadOffset);

    m_ReadOffset += 8;
    m_DataLength -= 8;

    if (m_ReadOffset >= m_FixedBlockSize)
    {
      m_CurBlock = m_Allocator->GetNextBlock(m_CurBlock);
      m_ReadOffset = 0;
    }

    return v;
  }

  void StormMessageReaderCursor::ReadByteBlock(void * buffer, int length)
  {
    if (m_DataLength < length)
    {
      throw std::runtime_error("Read buffer underflow");
    }

    m_DataLength -= length;

    while (m_ReadOffset + length > m_FixedBlockSize)
    {
      int copy_len = m_FixedBlockSize - m_ReadOffset;
      memcpy(buffer, Marshal::MemOffset(m_CurBlock, m_ReadOffset), copy_len);

      length -= copy_len;
      buffer = Marshal::MemOffset(buffer, copy_len);

      m_CurBlock = m_Allocator->GetNextBlock(m_CurBlock);
      m_ReadOffset = 0;
    }

    memcpy(buffer, Marshal::MemOffset(m_CurBlock, m_ReadOffset), length);
    m_ReadOffset += length;
  }

  void StormMessageReaderCursor::SkipWhiteSpace()
  {
    while (m_DataLength > 0)
    {
      auto b = PeekByte();

      if (b == ' ' || b == '\t' || b == '\r' || b == '\n')
      {
        Advance(1);
      }
      else
      {
        return;
      }
    }
  }

  bool StormMessageReaderCursor::ReadNumber(int & value, int required_digits)
  {
    if (GetRemainingLength() == 0)
    {
      return false;
    }

    StormMessageReaderCursor reader_copy = *this;

    char digit = reader_copy.ReadByte();
    if (digit < '0' || digit > '9')
    {
      return false;
    }

    value = 0;
    while (reader_copy.GetRemainingLength() > 0)
    {
      if (required_digits >= 0)
      {
        required_digits--;
        if (required_digits < 0)
        {
          return false;
        }
      }

      value *= 10;
      value += digit - '0';

      StormMessageReaderCursor prev_reader = reader_copy;

      digit = reader_copy.ReadByte();
      if (digit < '0' || digit > '9')
      {
        if (required_digits > 0)
        {
          return false;
        }

        *this = prev_reader;
        return true;
      }
    }

    if (required_digits <= 0)
    {
      value *= 10;
      value += digit - '0';

      *this = reader_copy;
      return true;
    }

    return false;
  }

  bool StormMessageReaderCursor::ReadHexNumber(int & value, int required_digits)
  {
    if (GetRemainingLength() == 0)
    {
      return false;
    }

    StormMessageReaderCursor reader_copy = *this;

    char digit = reader_copy.ReadByte();
    if ((digit < '0' || digit > '9') && (digit < 'a' || digit > 'f') && (digit < 'A' || digit > 'F'))
    {
      return false;
    }

    value = 0;
    while (true)
    {
      if (required_digits > 0)
      {
        required_digits--;
      }

      value *= 16;
      if (digit >= '0' && digit <= '9')
      {
        value += digit - '0';
      }
      else if (digit >= 'a' && digit <= 'f')
      {
        value += digit - 'a' + 10;
      }
      else if (digit >= 'A' && digit <= 'F')
      {
        value += digit - 'A' + 10;
      }

      if (reader_copy.GetRemainingLength() == 0)
      {
        *this = reader_copy;
        return true;
      }

      StormMessageReaderCursor prev_reader = reader_copy;

      digit = reader_copy.ReadByte();
      if ((digit < '0' || digit > '9') && (digit < 'a' || digit > 'f') && (digit < 'A' || digit > 'F'))
      {
        if (required_digits > 0)
        {
          return false;
        }

        *this = prev_reader;
        return true;
      }
    }

    return false;
  }

  uint32_t StormMessageReaderCursor::HashRemainingData(bool include_spaces)
  {
    Hash hash = crc32begin();
    while (m_DataLength > 0)
    {
      char c = PeekByte();
      if (c == ' ' && !include_spaces)
      {
        continue;
      }

      hash = crc32additive(hash, tolower(c));
      Advance(1);
    }

    return crc32end(hash);
  }


  uint32_t StormMessageReaderCursor::HashUntilDelimiter(char delimiter)
  {
    Hash hash = crc32begin();
    while (m_DataLength > 0)
    {
      char c = PeekByte();
      if (c == delimiter)
      {
        return crc32end(hash);
      }

      hash = crc32additive(hash, c);
      Advance(1);
    }

    return crc32end(hash);
  }

  void StormMessageReaderCursor::Advance(int num_bytes)
  {
    if (m_DataLength < num_bytes)
    {
      throw std::runtime_error("Read buffer underflow");
    }

    m_ReadOffset += num_bytes;
    m_DataLength -= num_bytes;

    while (m_ReadOffset >= m_FixedBlockSize)
    {
      m_CurBlock = m_Allocator->GetNextBlock(m_CurBlock);
      m_ReadOffset -= m_FixedBlockSize;
    }
  }
}