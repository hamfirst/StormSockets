
#include "StormSocketFrontendHttpBase.h"
#include "StormSocketLog.h"

namespace StormSockets
{
  StormSocketFrontendHttpBase::StormSocketFrontendHttpBase(const StormSocketFrontendHttpSettings & settings, StormSocketBackend * backend) :
    StormSocketFrontendBase(settings, backend)
  {

  }

  bool StormSocketFrontendHttpBase::ProcessHttpData(StormSocketConnectionBase & connection, StormHttpConnectionBase & http_connection, StormSocketConnectionId connection_id)
  {
    while (true)
    {
      if (http_connection.m_State == StormSocketClientConnectionHttpState::ReadingChunkSize)
      {
        ProfileScope prof(ProfilerCategory::kProcChunkSize);
        StormMessageHeaderReader header_reader(&m_Allocator, m_Allocator.ResolveHandle(connection.m_ParseBlock), connection.m_UnparsedDataLength, connection.m_ParseOffset);

        int full_data_len = 0;
        bool got_header = false;
        StormMessageReaderCursor cur_header = header_reader.AdvanceToNextHeader(full_data_len, got_header);

        if (got_header)
        {
          if (cur_header.ReadHexNumber(http_connection.m_ChunkSize) == false)
          {
            StormSocketLog("Got invalid chunk size\n");
            ForceDisconnect(connection_id);
            return true;
          }

          DiscardParsedData(connection_id, http_connection, full_data_len);
          void * parse_block = m_Allocator.ResolveHandle(connection.m_ParseBlock);

          AddBodyBlock(connection_id, http_connection, parse_block, http_connection.m_ChunkSize, connection.m_ParseOffset);
          http_connection.m_State = StormSocketClientConnectionHttpState::ReadingChunk;
        }
        else
        {
          return true;
        }
      }

      if (http_connection.m_State == StormSocketClientConnectionHttpState::ReadingChunk)
      {
        ProfileScope prof(ProfilerCategory::kProcChunk);
        if (connection.m_UnparsedDataLength >= http_connection.m_ChunkSize + 2)
        {
          if (http_connection.m_ChunkSize == 0)
          {
            if (CompleteBody(connection_id, http_connection) == false)
            {
              return false;
            }

            return true;
          }
          else
          {
            http_connection.m_State = StormSocketClientConnectionHttpState::ReadingChunkSize;
            DiscardParsedData(connection_id, http_connection, http_connection.m_ChunkSize + 2);
          }
        }
        else
        {
          return true;
        }
      }

      if (http_connection.m_State == StormSocketClientConnectionHttpState::ReadingBody)
      {
        if (http_connection.m_BodyLength > 0 && connection.m_UnparsedDataLength >= http_connection.m_BodyLength)
        {
          void * parse_block = m_Allocator.ResolveHandle(connection.m_ParseBlock);

          AddBodyBlock(connection_id, http_connection, parse_block, http_connection.m_BodyLength, connection.m_ParseOffset);
          if (CompleteBody(connection_id, http_connection) == false)
          {
            return false;
          }

          return true;
        }
        else
        {
          return true;
        }
      }
    }
  }

  void StormSocketFrontendHttpBase::DiscardParsedData(StormSocketConnectionId connection_id, StormHttpConnectionBase & http_connection, int amount)
  {
    http_connection.m_TotalLength += amount;
    m_Backend->DiscardParserData(connection_id, amount);
  }

}
