
#pragma once

namespace StormSockets
{
	struct StormGenIndex
	{
		volatile unsigned int Raw;

		static const int IndexMask = 0x00FFFFFF;
		static const int GenMask = 0xFF000000;

		StormGenIndex()
		{
			Raw = 0;
		}

		StormGenIndex(int index, int gen)
		{
			int v = gen & 0x000000FF;
			Raw = (index & 0x00FFFFFF) | (v << 24);
		}

		StormGenIndex(const StormGenIndex & rhs)
		{
			Raw = rhs.Raw;
		}

		int GetIndex() const
		{
			int raw = Raw;
			if ((raw & 0x00800000) != 0) // If the top bit is set, sign extend as a negative number
			{
				return (raw & IndexMask) | GenMask;
			}

			return raw & IndexMask;
		}

		int GetGen() const
		{
			return (int)(Raw >> 24) & 0xFF;
		}
	};

	struct StormDoubleGenIndex
	{
		volatile unsigned int Raw;

		static const int IndexMask = 0x000FFFFF;
		static const int Gen1Mask = 0xFF000000;
		static const int Gen2Mask = 0x00F00000;
		static const int DoubleGenMask = Gen1Mask | Gen2Mask;

		StormDoubleGenIndex()
		{
			Raw = 0;
		}

		StormDoubleGenIndex(int index, int gen1, int gen2)
		{
			int v1 = gen1 & 0x000000FF;
			int v2 = gen2 & 0x0000000F;
			Raw = (index & 0x000FFFFF) | (v1 << 24) | (v2 << 20);
		}

    StormDoubleGenIndex(const StormDoubleGenIndex & rhs)
    {
      Raw = rhs.Raw;
    }

		int GetIndex() const
		{
			int raw = Raw;
			if ((raw & 0x00080000) != 0) // If the top bit is set, sign extend as a negative number
			{
				return (raw & IndexMask) | (DoubleGenMask); // Set all the other bits
			}

			return raw & IndexMask;
		}


		int GetGen1() const
		{
			return (int)(Raw >> 24) & 0xFF;
		}


		int GetGen2() const
		{
			return (int)(Raw >> 20) & 0x0F;
		}
	};
}
