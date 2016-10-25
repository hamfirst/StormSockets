#pragma once

#include "StormMessageWriter.h"

namespace StormSockets
{
  class StormHttpRequestWriter
  {
    StormMessageWriter m_HeaderWriter;
    StormMessageWriter m_BodyWriter;

    friend class StormSocketBackend;

  public:
    StormHttpRequestWriter(const char * method, const char * uri, const char * host, StormMessageWriter & header_writer, StormMessageWriter & body_writer);
    StormHttpRequestWriter(const StormHttpRequestWriter & rhs) = default;
    StormHttpRequestWriter(StormHttpRequestWriter && rhs) = default;

    StormHttpRequestWriter & operator =(const StormHttpRequestWriter & rhs) = default;
    StormHttpRequestWriter & operator =(StormHttpRequestWriter && rhs) = default;

    void WriteHeader(const char * str);
    void WriteHeaders(const void * data, unsigned int len);
    void WriteBody(const void * data, unsigned int len);

    void FinalizeHeaders(bool write_content_len = true);

    StormMessageWriter & GetHeaderWriter() { return m_HeaderWriter; }
    StormMessageWriter & GetBodyWriter() { return m_BodyWriter; }
  };
}
