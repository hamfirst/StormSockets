
#include "StormHttpRequestWriter.h"

#include <stdio.h>
#include <string.h>

namespace StormSockets
{
  static const uint16_t line_ending = 0x0A0D;

  static const char * http_ver = "HTTP/1.1";
  static const char * host_header = "Host: ";
  static const char * user_agent = "Connection: close\r\nPragma: no-cache\r\nCache-Control: no-cache\r\nAccept: */*\r\n";
  static const char * content_len = "Content-Length: ";

  StormHttpRequestWriter::StormHttpRequestWriter(const char * method, const char * uri, const char * host, StormMessageWriter & header_writer, StormMessageWriter & body_writer)
  {
    m_HeaderWriter = header_writer;
    m_BodyWriter = body_writer;

    m_HeaderWriter.WriteString(method);
    m_HeaderWriter.WriteByte(' ');
    m_HeaderWriter.WriteString(uri);
    m_HeaderWriter.WriteByte(' ');
    m_HeaderWriter.WriteString(http_ver);
    m_HeaderWriter.WriteInt16(line_ending);
    m_HeaderWriter.WriteString(host_header);
    m_HeaderWriter.WriteString(host);
    m_HeaderWriter.WriteInt16(line_ending);
    m_HeaderWriter.WriteString(user_agent);
  }

  void StormHttpRequestWriter::WriteHeader(const char * str)
  {
    m_HeaderWriter.WriteString(str);
    m_HeaderWriter.WriteInt16(line_ending);
  }

  void StormHttpRequestWriter::WriteBody(const void * data, unsigned int len)
  {
    m_BodyWriter.WriteByteBlock(data, 0, len);
  }

  void StormHttpRequestWriter::WriteHeaders(const void * data, unsigned int len)
  {
    m_HeaderWriter.WriteByteBlock(data, 0, len);
  }

  void StormHttpRequestWriter::FinalizeHeaders(bool write_content_len)
  {
    if (write_content_len && m_BodyWriter.GetLength() > 0)
    {
      int body_len = m_BodyWriter.GetLength();
      char body_len_str[40];

      int str_len = snprintf(body_len_str, sizeof(body_len_str), "%d\r\n", body_len);
      m_HeaderWriter.WriteByteBlock(content_len, 0, strlen(content_len));
      m_HeaderWriter.WriteByteBlock(body_len_str, 0, str_len);
    }

    m_HeaderWriter.WriteInt16(line_ending);
  }
}
