#include "StormMessageHeaderReader.h"

#include <stdexcept>

namespace StormSockets
{
  StormMessageHeaderReader::StormMessageHeaderReader(StormFixedBlockAllocator * allocator, void * cur_block, int data_length, int read_offset) :
    StormMessageReaderCursor(allocator, cur_block, data_length, read_offset)
  {

  }

	StormMessageReaderCursor StormMessageHeaderReader::AdvanceToNextHeader(int & full_data_length, bool & found_complete_header)
	{
    auto start_block = m_CurBlock;
    auto start_offset = m_ReadOffset;

		int num_read = 0;
		full_data_length = 0;
		while (GetRemainingLength() > 0)
		{
			char v = ReadByte();
			full_data_length++;

			if (v == '\n')
			{
        found_complete_header = true;
				return StormMessageReaderCursor(m_Allocator, start_block, num_read, start_offset);
			}

			if (v != '\r')
			{
				num_read++;
			}
		}

    found_complete_header = false;
		return StormMessageReaderCursor(m_Allocator, start_block, 0, start_offset);
	}
}