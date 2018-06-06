#pragma once

#include "StormMessageReaderCursor.h"

namespace StormSockets
{
	struct StormMessageHeaderReader : public StormMessageReaderCursor
	{
    friend class StormSocketFrontendWebsocketBase;
    friend class StormSocketClientFrontendHttp;

    StormMessageHeaderReader(StormFixedBlockAllocator * allocator, void * cur_block, int data_length, int read_offset);

	public:
    StormMessageHeaderReader() = default;
    StormMessageHeaderReader(const StormMessageHeaderReader & rhs) = default;

    StormMessageReaderCursor AdvanceToNextHeader(int & full_data_length, bool & found_complete_header);
	};
}
