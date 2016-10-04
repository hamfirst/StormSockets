
#include "StormMessageHeaderValues.h"

namespace StormSockets
{
  void StormMessageHeaderValues::Init(std::vector<std::string> & headers)
  {
    for (int index = 0; index < (int)headers.size(); index++)
    {
      m_DataMatch.emplace_back(StormMessageHeaderValueMatch{ *((int *)headers[index].c_str()), headers[index] });
    }
  }

  void StormMessageHeaderValues::ReadInitialVal(StormMessageReaderCursor & reader, int & header_val, int & header_val_lowercase)
  {
    if (reader.GetRemainingLength() < 4)
    {
      header_val = 0;
      header_val_lowercase = 0;
      return;
    }

    header_val = reader.ReadInt32();
    header_val_lowercase = 0;

    for (int index = 0; index < 4; index++)
    {
      int b = (header_val >> (index * 8)) & 0xFF;

      // Lower case it
      if (b >= 'A' && b <= 'Z')
      {
        b += 'a' - 'A';
      }

      header_val_lowercase |= b << (index * 8);
    }
  }

	bool StormMessageHeaderValues::Match(StormMessageReaderCursor & reader, int initial_value, int type)
	{
		if (reader.GetRemainingLength() + 4 < (int)m_DataMatch[(int)type].m_Data.length())
		{
			return false;
		}

    StormMessageReaderCursor reader_copy = reader;

		int check_val = m_DataMatch[(int)type].m_InitialVal;
		if (initial_value != check_val)
		{
			return false;
		}

		int check_start = 4;
		int check_end = (int)m_DataMatch[(int)type].m_Data.length();

		for (int index = check_start; index < check_end; index++)
		{
			// Lower case it
			uint8_t b = reader_copy.ReadByte();
			if (b >= 'A' && b <= 'Z')
			{
				b += 'a' - 'A';
			}

			if (b != m_DataMatch[(int)type].m_Data[index])
			{
				return false;
			}
		}

    reader = reader_copy;
		return true;
	}

  bool StormMessageHeaderValues::MatchCaseSensitive(StormMessageReaderCursor & reader, int initial_value, int type)
  {
    if (reader.GetRemainingLength() + 4 < (int)m_DataMatch[(int)type].m_Data.length())
    {
      return false;
    }

    StormMessageReaderCursor reader_copy = reader;

    int check_val = m_DataMatch[(int)type].m_InitialVal;
    if (initial_value != check_val)
    {
      return false;
    }

    int check_start = 4;
    int check_end = (int)m_DataMatch[(int)type].m_Data.length();

    for (int index = check_start; index < check_end; index++)
    {
      uint8_t b = reader_copy.ReadByte();
      if (b != m_DataMatch[(int)type].m_Data[index])
      {
        return false;
      }
    }

    reader = reader_copy;
    return true;
  }

	bool StormMessageHeaderValues::MatchExact(StormMessageReaderCursor & reader, int initial_value, int type)
	{
		if (reader.GetRemainingLength() + 4 != m_DataMatch[(int)type].m_Data.length())
		{
			return false;
		}

    StormMessageReaderCursor reader_copy = reader;

		int check_val = m_DataMatch[(int)type].m_InitialVal;
		if ((initial_value) != check_val)
		{
			return false;
		}

		int check_start = 4;
		int check_end = (int)m_DataMatch[(int)type].m_Data.length();

		for (int index = check_start; index < check_end; index++)
		{
			// Lower case it
			uint8_t b = reader_copy.ReadByte();
			if (b >= 'A' && b <= 'Z')
			{
				b += 'a' - 'A';
			}

			if (b != m_DataMatch[(int)type].m_Data[index])
			{
				return false;
			}
		}

    reader = reader_copy;
		return true;
	}

	bool StormMessageHeaderValues::MatchExactCaseSensitive(StormMessageReaderCursor & reader, int initial_value, int type)
	{
		if (reader.GetRemainingLength() + 4 != m_DataMatch[(int)type].m_Data.length())
		{
			return false;
		}

    StormMessageReaderCursor reader_copy = reader;

		int check_val = m_DataMatch[(int)type].m_InitialVal;
		if ((initial_value) != check_val)
		{
			return false;
		}

		int check_start = 4;
		int check_end = (int)m_DataMatch[(int)type].m_Data.length();

		for (int index = check_start; index < check_end; index++)
		{
			if (reader_copy.ReadByte() != m_DataMatch[(int)type].m_Data[index])
			{
				return false;
			}
		}

    reader = reader_copy;
		return true;
	}

	bool StormMessageHeaderValues::FindCSLValue(StormMessageReaderCursor & reader, int type)
	{
    StormMessageReaderCursor reader_copy = reader;

		while (reader_copy.GetRemainingLength() > 0)
		{
			int check_start = 0;
			int check_end = (int)m_DataMatch[(int)type].m_Data.length();

			bool matched = true;
			for (int index = check_start; index < check_end; index++)
			{
				// Lower case it
				uint8_t b = reader_copy.ReadByte();
				if (b >= 'A' && b <= 'Z')
				{
					b += 'a' - 'A';
				}

				if (b != m_DataMatch[(int)type].m_Data[index])
				{
					matched = false;
					break;
				}
			}

			if (matched)
			{
				if (reader_copy.GetRemainingLength() == 0 || reader_copy.ReadByte() == (uint8_t)',')
				{
					return true;
				}
			}

			// Find the next comma
			while (reader_copy.GetRemainingLength() > 0 && reader_copy.ReadByte() != (uint8_t)',')
			{

			}

			// Skip the whitespace
			if (reader_copy.GetRemainingLength() == 0 || reader_copy.ReadByte() != (uint8_t)' ')
			{
				return false;
			}
		}

		return false;
	}

	void StormMessageHeaderValues::WriteHeader(StormMessageWriter & writer, int type)
	{
		writer.WriteByteBlock(m_DataMatch[(int)type].m_Data.c_str(), 0, (int)m_DataMatch[(int)type].m_Data.length());
	}
}
