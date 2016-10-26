
#include "StormHttpResponseWriter.h"

#include <stdio.h>
#include <string.h>

namespace StormSockets
{
  static const uint16_t line_ending = 0x0A0D;

  static const char * http_ver = "HTTP/1.1 ";
  static const char * host_header = "Host: ";
  static const char * content_len = "Content-Length: ";

  StormHttpResponseWriter::StormHttpResponseWriter(int response_code, const char * response_phrase, StormMessageWriter & header_writer, StormMessageWriter & body_writer)
  {
    m_HeaderWriter = header_writer;
    m_BodyWriter = body_writer;

    char response_code_str[5];
    snprintf(response_code_str, 5, "%03d ", response_code);

    m_HeaderWriter.WriteString(http_ver);
    m_HeaderWriter.WriteByteBlock(response_code_str, 0, 4);
    m_HeaderWriter.WriteString(response_phrase);
    m_HeaderWriter.WriteInt16(line_ending);
  }

  void StormHttpResponseWriter::WriteHeader(const char * str)
  {
    while (*str != 0)
    {
      m_HeaderWriter.WriteByte(*str);
      str++;
    }

    m_HeaderWriter.WriteInt16(line_ending);
  }

  void StormHttpResponseWriter::WriteHeaders(const void * data, unsigned int len)
  {
    m_HeaderWriter.WriteByteBlock(data, 0, len);
  }

  void StormHttpResponseWriter::WriteBody(const void * data, unsigned int len)
  {
    m_BodyWriter.WriteByteBlock(data, 0, len);
  }

  void StormHttpResponseWriter::FinalizeHeaders(bool write_content_len)
  {
    if (write_content_len && m_BodyWriter.GetLength() > 0)
    {
      int body_len = m_BodyWriter.GetLength();
      char body_len_str[40];

      int str_len = snprintf(body_len_str, sizeof(body_len_str), "%d\r\n", body_len);
      m_HeaderWriter.WriteString(content_len);
      m_HeaderWriter.WriteString(body_len_str);
    }

    m_HeaderWriter.WriteInt16(line_ending);
  }

  void StormHttpResponseWriter::DebugPrint()
  {
    m_HeaderWriter.DebugPrint();
    m_BodyWriter.DebugPrint();
  }
}
