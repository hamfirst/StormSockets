#pragma once

#include "StormMessageWriter.h"

namespace StormSockets
{
  class StormHttpResponseWriter
  {
    StormMessageWriter m_HeaderWriter;
    StormMessageWriter m_BodyWriter;

    friend class StormSocketBackend;

  public:
    StormHttpResponseWriter(int response_code, const char * response_phrase, StormMessageWriter & header_writer, StormMessageWriter & body_writer);
    StormHttpResponseWriter(const StormHttpResponseWriter & rhs) = default;
    StormHttpResponseWriter(StormHttpResponseWriter && rhs) = default;

    StormHttpResponseWriter & operator =(const StormHttpResponseWriter & rhs) = default;
    StormHttpResponseWriter & operator =(StormHttpResponseWriter && rhs) = default;

    void WriteHeader(const char * str);
    void WriteHeaders(const void * data, unsigned int len);
    void WriteBody(const void * data, unsigned int len);

    void FinalizeHeaders(bool write_content_len = true);

    void DebugPrint();

    StormMessageWriter & GetHeaderWriter() { return m_HeaderWriter; }
    StormMessageWriter & GetBodyWriter() { return m_BodyWriter; }
  };
}
