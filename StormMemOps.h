
#pragma once

#include <stdint.h>
#include <cstring>

namespace StormSockets
{
	namespace Marshal
	{
		inline void * MemOffset(void * ptr, int ofs)
		{
			return ((uint8_t *)ptr) + ofs;
		}

    inline const void * MemOffset(const void * ptr, int ofs)
    {
      return ((uint8_t *)ptr) + ofs;
    }

		inline uint8_t ReadByte(void * ptr)
		{
			return *((uint8_t *)ptr);
		}

		inline uint8_t ReadByte(void * ptr, int ofs)
		{
			ptr = ((uint8_t *)ptr) + ofs;
			return *((uint8_t *)ptr);
		}

		inline uint16_t ReadInt16(void * ptr)
		{
			return *((uint16_t *)ptr);
		}

		inline uint16_t ReadInt16(void * ptr, int ofs)
		{
			ptr = ((uint8_t *)ptr) + ofs;
			return *((uint16_t *)ptr);
		}

		inline uint32_t ReadInt32(void * ptr)
		{
			return *((uint32_t *)ptr);
		}

		inline uint32_t ReadInt32(void * ptr, int ofs)
		{
			ptr = ((uint8_t *)ptr) + ofs;
			return *((uint32_t *)ptr);
		}

		inline uint64_t ReadInt64(void * ptr)
		{
			return *((uint64_t *)ptr);
		}

		inline uint64_t ReadInt64(void * ptr, int ofs)
		{
			ptr = ((uint8_t *)ptr) + ofs;
			return *((uint64_t *)ptr);
		}

		inline void WriteByte(void * ptr, uint8_t val)
		{
			*((uint8_t *)ptr) = val;
		}

		inline void WriteByte(void * ptr, int ofs, uint8_t val)
		{
			ptr = ((uint8_t *)ptr) + ofs;
			*((uint8_t *)ptr) = val;
		}

		inline void WriteInt16(void * ptr, uint16_t val)
		{
			*((uint16_t *)ptr) = val;
		}

		inline void WriteInt16(void * ptr, int ofs, uint16_t val)
		{
			ptr = ((uint8_t *)ptr) + ofs;
			*((uint16_t *)ptr) = val;
		}

		inline void WriteInt32(void * ptr, uint32_t val)
		{
			*((uint32_t *)ptr) = val;
		}

		inline void WriteInt32(void * ptr, int ofs, uint32_t val)
		{
			ptr = ((uint8_t *)ptr) + ofs;
			*((uint32_t *)ptr) = val;
		}

		inline void WriteInt64(void * ptr, uint64_t val)
		{
			*((uint64_t *)ptr) = val;
		}

		inline void WriteInt64(void * ptr, int ofs, uint64_t val)
		{
			ptr = ((uint8_t *)ptr) + ofs;
			*((uint64_t *)ptr) = val;
		}

		inline uint16_t HostToNetworkOrder(uint16_t val)
		{
			return (val << 8) | (val >> 8);
		}

		inline uint32_t HostToNetworkOrder(uint32_t val)
		{
			return (val << 24) | ((val << 8) & 0x00FF0000) | ((val >> 8) & 0x0000FF00) | (val >> 24);
		}

		inline uint64_t HostToNetworkOrder(uint64_t val)
		{
			val = ((val << 8) & 0xFF00FF00FF00FF00ULL) | ((val >> 8) & 0x00FF00FF00FF00FFULL);
			val = ((val << 16) & 0xFFFF0000FFFF0000ULL) | ((val >> 16) & 0x0000FFFF0000FFFFULL);
			return (val << 32) | (val >> 32);
		}

		inline void Copy(void * ptr, int ofs, const void * buffer, int length)
		{
			ptr = ((uint8_t *)ptr) + ofs;
			std::memcpy(ptr, buffer, length);
		}
	}
}
