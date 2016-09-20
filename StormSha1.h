#pragma once

#include <stdint.h>
#include <string>
#include <exception>

#include <StormSockets\StormMessageHeaderReader.h>
#include <StormSockets\StormMessageWriter.h>

namespace StormSockets
{
	class StormSha1
	{
	public:
		static void CalcHash(const StormMessageReaderCursor & header_reader, StormMessageWriter & writer);
    static void CalcHash(const char * src, std::string & output);
	};
}
