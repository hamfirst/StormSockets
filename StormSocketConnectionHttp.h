#pragma once

#include "StormSocketConnection.h"
#include "StormHttpResponseReader.h"
#include "StormMessageReaderCursor.h"
#include "StormMessageHeaderReader.h"

namespace StormSockets
{
  namespace StormSocketClientConnectionHttpState
  {
    enum Index
    {
      ReadingHeaders,
      ReadingChunkSize,
      ReadingChunk,
      ReadingBody
    };
  }

  struct StormHttpConnectionBase
  {
    StormSocketClientConnectionHttpState::Index m_State = StormSocketClientConnectionHttpState::ReadingHeaders;
    bool m_Chunked = false;
    int m_TotalLength = 0;
    int m_BodyLength = -1;
    int m_ChunkSize = 0;
    int m_RecievedLength = 0;

    std::experimental::optional<StormMessageHeaderReader> m_Headers;
  };

  struct StormSocketClientConnectionHttp : public StormHttpConnectionBase
  {
    std::experimental::optional<StormHttpRequestWriter> m_RequestWriter;

    bool m_UseSSL = false;

    bool m_GotStatusLine = false;
    bool m_CompleteResponse = false;

    int m_ResponseCode = 0;

    std::experimental::optional<StormMessageReaderCursor> m_StatusLine;
    std::experimental::optional<StormMessageReaderCursor> m_ResponsePhrase;
    std::experimental::optional<StormHttpResponseReader> m_BodyReader;
  };

  struct StormSocketServerConnectionHttp : public StormHttpConnectionBase
  {
    bool m_GotRequestLine = false;
    bool m_CompleteRequest = false;

    std::experimental::optional<StormMessageReaderCursor> m_RequestMethod;
    std::experimental::optional<StormMessageReaderCursor> m_RequestURI;
    std::experimental::optional<StormHttpRequestReader> m_BodyReader;
  };
}
