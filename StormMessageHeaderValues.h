
#pragma once

#include <string>
#include <vector>

#include "StormMessageReaderCursor.h"
#include "StormMessageWriter.h"

namespace StormSockets
{
	struct StormMessageHeaderValueMatch
	{
		int m_InitialVal;
		std::string m_Data;
	};

	struct StormMessageHeaderValues
	{
  protected:

    void Init(std::vector<std::string> & headers);

  public:

    void ReadInitialVal(StormMessageReaderCursor & reader, int & header_val, int & header_val_lowercase);

		bool Match(StormMessageReaderCursor & reader, int initial_value, int type);
    bool MatchCaseSensitive(StormMessageReaderCursor & reader, int initial_value, int type);
		bool MatchExact(StormMessageReaderCursor & reader, int initial_value, int type);
		bool MatchExactCaseSensitive(StormMessageReaderCursor & reader, int initial_value, int type);

		bool FindCSLValue(StormMessageReaderCursor & reader, int type);

		void WriteHeader(StormMessageWriter & writer, int type);

  protected:

    std::vector<StormMessageHeaderValueMatch> m_DataMatch;
	};
}
