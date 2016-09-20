#pragma once

#include <StormSockets\StormMessageWriter.h>

namespace StormSockets
{
  class StormHttpResponseWriter
  {
    StormMessageWriter m_HeaderWriter;
    StormMessageWriter m_BodyWriter;

    friend class StormSocketBackend;

  public:
    StormHttpResponseWriter(int response_code, char * response_phrase, StormMessageWriter & header_writer, StormMessageWriter & body_writer);
    StormHttpResponseWriter(const StormHttpResponseWriter & rhs) = default;
    StormHttpResponseWriter(StormHttpResponseWriter && rhs) = default;

    StormHttpResponseWriter & operator =(const StormHttpResponseWriter & rhs) = default;
    StormHttpResponseWriter & operator =(StormHttpResponseWriter && rhs) = default;

    void WriteHeader(const char * str);
    void WriteBody(void * data, unsigned int len);

    void FinalizeHeaders(bool write_content_len = true);

    StormMessageWriter & GetHeaderWriter() { return m_HeaderWriter; }
    StormMessageWriter & GetBodyWriter() { return m_BodyWriter; }
  };
}
