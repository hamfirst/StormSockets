#include "StormSha1.h"

#include <stdexcept>

#include <string.h>

namespace StormSockets
{
	// "258EAFA5-E914-47DA-95CA-C5AB0DC85B11" as a uint8_t array
	static uint8_t s_WebsocketGuid[] = {
		0x32, 0x35, 0x38, 0x45, 0x41, 0x46, 0x41, 0x35,
		0x2D, 0x45, 0x39, 0x31, 0x34, 0x2D, 0x34, 0x37,
		0x44, 0x41, 0x2D, 0x39, 0x35, 0x43, 0x41, 0x2D,
		0x43, 0x35, 0x41, 0x42, 0x30, 0x44, 0x43, 0x38,
		0x35, 0x42, 0x31, 0x31 };

	struct StormMessageHeaderReaderSha1
	{
    StormMessageReaderCursor m_Reader;
		int m_GuidPos;

		uint8_t ReadByte()
		{
			if (m_Reader.GetRemainingLength() == 0)
			{
				if (m_GuidPos >= sizeof(s_WebsocketGuid))
				{
					throw std::runtime_error("Read buffer underflow");
				}

				int pos = m_GuidPos;
				m_GuidPos++;
				return s_WebsocketGuid[pos];
			}

			return m_Reader.ReadByte();
		}

    auto GetRemainingLength()
    {
      return m_Reader.GetRemainingLength();
    }
	};

  struct StormStringReaderSha1
  {
    const char * m_Str;
    int m_Len;
    int m_GuidPos;

    uint8_t ReadByte()
    {
      if (m_Len == 0)
      {
        if (m_GuidPos >= sizeof(s_WebsocketGuid))
        {
          throw std::runtime_error("Read buffer underflow");
        }

        int pos = m_GuidPos;
        m_GuidPos++;
        return s_WebsocketGuid[pos];
      }

      m_Len--;
      uint8_t v = *m_Str;
      m_Str++;
      return v;
    }

    auto GetRemainingLength()
    {
      return m_Len;
    }
  };

  struct StormStringWriterSha1
  {
    std::string & m_Str;

    void WriteByte(uint8_t val)
    {
      m_Str.push_back(val);
    }
  };

	// Rotate an integer value to left.
	static uint32_t rol(uint32_t value, int steps)
	{
		return ((value << steps) | (value >> (32 - steps)));
	}

	// Sets the first 16 integers in the buffert to zero.
	// Used for clearing the W buffert.
	static void clearWBuffert(uint32_t* buffert)
	{
		for (int pos = 16; --pos >= 0;)
		{
			buffert[pos] = 0;
		}
	}

	static void innerHash(uint32_t* w, uint32_t & result1, uint32_t & result2, uint32_t & result3, uint32_t & result4, uint32_t & result5)
	{
		uint32_t a = result1;
		uint32_t b = result2;
		uint32_t c = result3;
		uint32_t d = result4;
		uint32_t e = result5;

		int round = 0;

		while (round < 16)
		{
			uint32_t t = rol(a, 5) + ((b & c) | (~b & d)) + e + 0x5a827999 + w[round];
			e = d;
			d = c;
			c = rol(b, 30);
			b = a;
			a = t;
			++round;
		}
		while (round < 20)
		{
			w[round] = rol((w[round - 3] ^ w[round - 8] ^ w[round - 14] ^ w[round - 16]), 1);
			uint32_t t = rol(a, 5) + ((b & c) | (~b & d)) + e + 0x5a827999 + w[round];
			e = d;
			d = c;
			c = rol(b, 30);
			b = a;
			a = t;

			++round;
		}
		while (round < 40)
		{
			w[round] = rol((w[round - 3] ^ w[round - 8] ^ w[round - 14] ^ w[round - 16]), 1);

			uint32_t t = rol(a, 5) + (b ^ c ^ d) + e + 0x6ed9eba1 + w[round];
			e = d;
			d = c;
			c = rol(b, 30);
			b = a;
			a = t;

			++round;
		}
		while (round < 60)
		{
			w[round] = rol((w[round - 3] ^ w[round - 8] ^ w[round - 14] ^ w[round - 16]), 1);

			uint32_t t = rol(a, 5) + ((b & c) | (b & d) | (c & d)) + e + 0x8f1bbcdc + w[round];
			e = d;
			d = c;
			c = rol(b, 30);
			b = a;
			a = t;

			++round;
		}
		while (round < 80)
		{
			w[round] = rol((w[round - 3] ^ w[round - 8] ^ w[round - 14] ^ w[round - 16]), 1);
			uint32_t t = rol(a, 5) + (b ^ c ^ d) + e + 0xca62c1d6 + w[round];
			e = d;
			d = c;
			c = rol(b, 30);
			b = a;
			a = t;


			++round;
		}

		result1 += a;
		result2 += b;
		result3 += c;
		result4 += d;
		result5 += e;
	}

  template <class Reader, class Writer>
	void CalcHashAlgorithm(Reader && reader, Writer && writer)
	{
		uint32_t result1 = 0x67452301;
		uint32_t result2 = 0xefcdab89;
		uint32_t result3 = 0x98badcfe;
		uint32_t result4 = 0x10325476;
		uint32_t result5 = 0xc3d2e1f0;

		uint32_t w[80];

		// Loop through all complete 64uint8_t blocks.
		int uint8_tlength = reader.GetRemainingLength() + sizeof(s_WebsocketGuid);
		int endOfFullBlocks = uint8_tlength - 64;
		int endCurrentBlock;
		int currentBlock = 0;

		while (currentBlock <= endOfFullBlocks)
		{
			endCurrentBlock = currentBlock + 64;

			// Init the round buffer with the 64 uint8_t block data.
			for (int roundPos = 0; currentBlock < endCurrentBlock; currentBlock += 4)
			{
				uint8_t b1 = reader.ReadByte();
				uint8_t b2 = reader.ReadByte();
				uint8_t b3 = reader.ReadByte();
				uint8_t b4 = reader.ReadByte();

				// This line will swap endian on big endian and keep endian on little endian.
				w[roundPos++] = (uint32_t)b4
					| (((uint32_t)b3) << 8)
					| (((uint32_t)b2) << 16)
					| (((uint32_t)b1) << 24);
			}
			innerHash(w, result1, result2, result3, result4, result5);
		}

		// Handle the last and not full 64 uint8_t block if existing.
		endCurrentBlock = uint8_tlength - currentBlock;
		clearWBuffert(w);
		int lastBlockBytes = 0;
		for (; lastBlockBytes < endCurrentBlock; ++lastBlockBytes)
		{
			w[lastBlockBytes >> 2] |= (uint32_t)reader.ReadByte() << ((3 - (lastBlockBytes & 3)) << 3);
		}
		w[lastBlockBytes >> 2] |= (uint32_t)(0x80 << ((3 - (lastBlockBytes & 3)) << 3));
		if (endCurrentBlock >= 56)
		{
			innerHash(w, result1, result2, result3, result4, result5);
			clearWBuffert(w);
		}
		w[15] = (uint32_t)(uint8_tlength << 3);
		innerHash(w, result1, result2, result3, result4, result5);

		uint8_t b = 0;
		uint8_t cur = 0;
		b = (uint8_t)((result1 >> 24) & 0xFF);
		cur = (uint8_t)((b >> 2) & 0x3F);
		b <<= 6;
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		cur = (uint8_t)((b >> 2) & 0x3F);
		b = (uint8_t)((result1 >> 16) & 0xFF);
		cur |= (uint8_t)((b >> 4) & 0x3F);
		b <<= 4;
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		cur = (uint8_t)((b >> 2) & 0x3F);
		b = (uint8_t)((result1 >> 8) & 0xFF);
		cur |= (uint8_t)((b >> 6) & 0x3F);
		b <<= 2;
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		cur = (uint8_t)((b >> 2) & 0x3F);
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		b = (uint8_t)((result1 >> 0) & 0xFF);
		cur = (uint8_t)((b >> 2) & 0x3F);
		b <<= 6;
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		cur = (uint8_t)((b >> 2) & 0x3F);
		b = (uint8_t)((result2 >> 24) & 0xFF);
		cur |= (uint8_t)((b >> 4) & 0x3F);
		b <<= 4;
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		cur = (uint8_t)((b >> 2) & 0x3F);
		b = (uint8_t)((result2 >> 16) & 0xFF);
		cur |= (uint8_t)((b >> 6) & 0x3F);
		b <<= 2;
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		cur = (uint8_t)((b >> 2) & 0x3F);
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		b = (uint8_t)((result2 >> 8) & 0xFF);
		cur = (uint8_t)((b >> 2) & 0x3F);
		b <<= 6;
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		cur = (uint8_t)((b >> 2) & 0x3F);
		b = (uint8_t)((result2 >> 0) & 0xFF);
		cur |= (uint8_t)((b >> 4) & 0x3F);
		b <<= 4;
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		cur = (uint8_t)((b >> 2) & 0x3F);
		b = (uint8_t)((result3 >> 24) & 0xFF);
		cur |= (uint8_t)((b >> 6) & 0x3F);
		b <<= 2;
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		cur = (uint8_t)((b >> 2) & 0x3F);
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		b = (uint8_t)((result3 >> 16) & 0xFF);
		cur = (uint8_t)((b >> 2) & 0x3F);
		b <<= 6;
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		cur = (uint8_t)((b >> 2) & 0x3F);
		b = (uint8_t)((result3 >> 8) & 0xFF);
		cur |= (uint8_t)((b >> 4) & 0x3F);
		b <<= 4;
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		cur = (uint8_t)((b >> 2) & 0x3F);
		b = (uint8_t)((result3 >> 0) & 0xFF);
		cur |= (uint8_t)((b >> 6) & 0x3F);
		b <<= 2;
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		cur = (uint8_t)((b >> 2) & 0x3F);
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		b = (uint8_t)((result4 >> 24) & 0xFF);
		cur = (uint8_t)((b >> 2) & 0x3F);
		b <<= 6;
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		cur = (uint8_t)((b >> 2) & 0x3F);
		b = (uint8_t)((result4 >> 16) & 0xFF);
		cur |= (uint8_t)((b >> 4) & 0x3F);
		b <<= 4;
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		cur = (uint8_t)((b >> 2) & 0x3F);
		b = (uint8_t)((result4 >> 8) & 0xFF);
		cur |= (uint8_t)((b >> 6) & 0x3F);
		b <<= 2;
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		cur = (uint8_t)((b >> 2) & 0x3F);
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		b = (uint8_t)((result4 >> 0) & 0xFF);
		cur = (uint8_t)((b >> 2) & 0x3F);
		b <<= 6;
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		cur = (uint8_t)((b >> 2) & 0x3F);
		b = (uint8_t)((result5 >> 24) & 0xFF);
		cur |= (uint8_t)((b >> 4) & 0x3F);
		b <<= 4;
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		cur = (uint8_t)((b >> 2) & 0x3F);
		b = (uint8_t)((result5 >> 16) & 0xFF);
		cur |= (uint8_t)((b >> 6) & 0x3F);
		b <<= 2;
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		cur = (uint8_t)((b >> 2) & 0x3F);
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		b = (uint8_t)((result5 >> 8) & 0xFF);
		cur = (uint8_t)((b >> 2) & 0x3F);
		b <<= 6;
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		cur = (uint8_t)((b >> 2) & 0x3F);
		b = (uint8_t)((result5 >> 0) & 0xFF);
		cur |= (uint8_t)((b >> 4) & 0x3F);
		b <<= 4;
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		cur = (uint8_t)((b >> 2) & 0x3F);
		b = 0;
		cur |= (uint8_t)((b >> 6) & 0x3F);
		b <<= 2;
		if (cur < 26) writer.WriteByte((uint8_t)(cur + 65));
		else if (cur < 52) writer.WriteByte((uint8_t)(cur + 71));
		else if (cur < 62) writer.WriteByte((uint8_t)(cur - 4));
		else if (cur < 63) writer.WriteByte((uint8_t)43);
		else writer.WriteByte((uint8_t)47);
		writer.WriteByte((uint8_t)'=');
	}

  void StormSha1::CalcHash(const StormMessageReaderCursor & header_reader, StormMessageWriter & writer)
  {
    StormMessageHeaderReaderSha1 reader = { header_reader , 0 };
    CalcHashAlgorithm(reader, writer);
  }

  void StormSha1::CalcHash(const char * src, std::string & output)
  {
    StormStringReaderSha1 reader = { src, (int)strlen(src), 0 };
    StormStringWriterSha1 writer = { output };

    CalcHashAlgorithm(reader, writer);
  }
}