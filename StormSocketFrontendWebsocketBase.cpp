
#include "StormSocketFrontendWebsocketBase.h"

#include <stdexcept>

namespace StormSockets
{

  StormSocketFrontendWebsocketBase::StormSocketFrontendWebsocketBase(const StormSocketFrontendWebsocketSettings & settings, StormSocketBackend * backend) :
    StormSocketFrontendBase(settings, backend)
  {
    m_UseMasking = settings.UseMasking;
    m_ContinuationMode = settings.ContinuationMode;
    m_MaxPacketSize = settings.MaxPacketSize;
  }

  StormWebsocketMessageWriter StormSocketFrontendWebsocketBase::CreateOutgoingPacket(StormSocketWebsocketDataType::Index type, bool final)
  {
    StormWebsocketOp::Index mode;
    switch (type)
    {
    case StormSocketWebsocketDataType::Binary:
    default:
      mode = StormWebsocketOp::BinaryFrame;
      break;
    case StormSocketWebsocketDataType::Text:
      mode = StormWebsocketOp::TextFrame;
      break;
    case StormSocketWebsocketDataType::Ping:
      mode = StormWebsocketOp::Ping;
      break;
    case StormSocketWebsocketDataType::Pong:
      throw std::runtime_error("not supported"); // Users cannot create Pong messages, they are created automatically in response to a ping
    case StormSocketWebsocketDataType::Continuation:
      mode = StormWebsocketOp::Continuation;
      break;
    }

    return CreateOutgoingPacket(mode, final);
  }

  StormWebsocketMessageWriter StormSocketFrontendWebsocketBase::CreateOutgoingPacket(StormWebsocketOp::Index mode, bool final)
  {
    StormWebsocketMessageWriter writer = m_Backend->CreateWriter();
    writer.SaveHeaderRoom();
    writer.m_Mode = mode;
    writer.m_Final = final;
    return writer;
  }

  void StormSocketFrontendWebsocketBase::FinalizeOutgoingPacket(StormWebsocketMessageWriter & writer)
  {
    uint64_t prof = Profiling::StartProfiler();
    writer.CreateHeaderAndApplyMask((int)writer.m_Mode, writer.m_Final, m_UseMasking ? rand() : 0);
    Profiling::EndProfiler(prof, ProfilerCategory::kFinalizePacket);
  }

  void StormSocketFrontendWebsocketBase::FreeIncomingPacket(StormWebsocketMessageReader & reader)
  {
    reader.FreeChain();
    m_Backend->DiscardReaderData(reader.m_ConnectionId, reader.m_PacketDataLen);
  }

  bool StormSocketFrontendWebsocketBase::ProcessWebsocketData(StormSocketConnectionBase & connection, StormWebsocketConnectionBase & ws_connection, StormSocketConnectionId connection_id)
  {
    while (true)
    {
      if (ws_connection.m_State == StormSocketServerConnectionWebsocketState::ReadHeaderAndApplyMask)
      {
        StormMessageHeaderReader cur_header(&m_Allocator, m_Allocator.ResolveHandle(connection.m_ParseBlock), connection.m_UnparsedDataLength, connection.m_ParseOffset);
        if (cur_header.GetRemainingLength() < 2) // Enough data for a header
        {
          return true;
        }

        uint8_t opdata = cur_header.ReadByte();
        uint8_t len1 = cur_header.ReadByte();
        int header_len = 2;

        if ((opdata & 0x70) != 0) // Reserved bits must be zero
        {
          m_Backend->SignalCloseThread(connection_id);
          return true;
        }

        bool fin = (opdata & 0x80) != 0;
        int opcode = opdata & 0x0F;

        bool mask_enabled = (len1 & 0x80) != 0;

        // Figure out how much data is in the packet
        uint64_t len = len1 & 0x7F;
        if (len == 126)
        {
          if (cur_header.GetRemainingLength() < 2)
          {
            return true;
          }

          uint16_t host_len = cur_header.ReadInt16();
          len = Marshal::HostToNetworkOrder(host_len);
          header_len += 2;
        }
        else if (len == 127)
        {
          if (cur_header.GetRemainingLength() < 8)
          {
            return true;
          }

          len = Marshal::HostToNetworkOrder(cur_header.ReadInt64());
          header_len += 8;
        }

        // Read the mask
        uint32_t mask = 0;
        if (mask_enabled)
        {
          if (cur_header.GetRemainingLength() < 4)
          {
            return true;
          }

          mask = (uint32_t)cur_header.ReadInt32();
          header_len += 4;
        }

        if ((uint64_t)cur_header.GetRemainingLength() < len)
        {
          return true;
        }

        // At this point, we have the full packet in memory

        int full_data_len = (int)len + header_len;

        if (m_MaxPacketSize > 0 && full_data_len > m_MaxPacketSize)
        {
          m_Backend->SignalCloseThread(connection_id);
          return true;
        }

        StormSocketWebsocketDataType::Index data_type = StormSocketWebsocketDataType::Binary;

        StormWebsocketOp::Index op = (StormWebsocketOp::Index)opcode;
        switch (op)
        {
        case StormWebsocketOp::Continuation:
          if (ws_connection.m_InContinuation == false)
          {
            m_Backend->SignalCloseThread(connection_id);
            return true;
          }
          data_type = StormSocketWebsocketDataType::Continuation;
          break;
        case StormWebsocketOp::BinaryFrame:
          if (ws_connection.m_InContinuation == true)
          {
            m_Backend->SignalCloseThread(connection_id);
            return true;
          }
          break;
        case StormWebsocketOp::TextFrame:
          if (ws_connection.m_InContinuation == true)
          {
            m_Backend->SignalCloseThread(connection_id);
            return true;
          }

          data_type = StormSocketWebsocketDataType::Text;
          break;
        case StormWebsocketOp::Pong:
          if (!fin || len > 125)
          {
            m_Backend->SignalCloseThread(connection_id);
            return true;
          }

          data_type = StormSocketWebsocketDataType::Pong;
          break;
        case StormWebsocketOp::Ping:
          if (!fin || len > 125)
          {
            m_Backend->SignalCloseThread(connection_id);
            return true;
          }

          data_type = StormSocketWebsocketDataType::Ping;
          break;
        case StormWebsocketOp::Close:
          if (!fin || len > 125)
          {
            m_Backend->SignalCloseThread(connection_id);
            return true;
          }

          m_Backend->SetDisconnectFlag(connection_id, StormSocketDisconnectFlags::kRemoteClose);
          m_Backend->SetDisconnectFlag(connection_id, StormSocketDisconnectFlags::kLocalClose);
          break;
        default:
          m_Backend->SignalCloseThread(connection_id);
          return true;
        }

        // Create the reader
        StormFixedBlockHandle cur_block_handle = m_Allocator.GetHandleForBlock(cur_header.m_CurBlock);
        StormWebsocketMessageReader reader(&m_Allocator, &m_MessageReaders, cur_block_handle, (int)len, cur_header.m_ReadOffset, connection_id, m_FixedBlockSize);
        reader.m_PacketDataLen = full_data_len;
        reader.m_FullDataLen = (int)len;
        reader.m_DataType = data_type;
        reader.m_FinalInSequence = fin;

        if (mask != 0)
        {
          // Apply the mask
          void * cur_block = cur_header.m_CurBlock;
          int read_offset = cur_header.m_ReadOffset;
          int data_length = (int)len;

          do
          {
            if (data_length >= 4 && read_offset + 4 <= m_FixedBlockSize)
            {
              int val = Marshal::ReadInt32(cur_block, read_offset);
              val ^= (int)mask;

              Marshal::WriteInt32(cur_block, read_offset, val);
              read_offset += 4;
              data_length -= 4;
            }
            else if (data_length > 0 && read_offset != m_FixedBlockSize)
            {
              int val = Marshal::ReadByte(cur_block, read_offset);
              val ^= (int)mask;

              Marshal::WriteByte(cur_block, read_offset, (uint8_t)val);
              read_offset += 1;
              data_length -= 1;

              mask = (mask << 24) | (mask >> 8);
            }

            if (read_offset == m_FixedBlockSize)
            {
              cur_block = m_Allocator.GetNextBlock(cur_block);
              read_offset = 0;
            }
          } while (data_length > 0 && cur_block != NULL);
        }

        if (op == StormWebsocketOp::Close)
        {
          m_Backend->DiscardParserData(connection_id, full_data_len);

          FreeIncomingPacket(reader);
          ws_connection.m_State = StormSocketServerConnectionWebsocketState::ReadHeaderAndApplyMask;
          return true;
        }
        else if (reader.m_DataType == StormSocketWebsocketDataType::Binary ||
          reader.m_DataType == StormSocketWebsocketDataType::Text ||
          reader.m_DataType == StormSocketWebsocketDataType::Continuation)
        {
          ws_connection.m_InContinuation = !reader.m_FinalInSequence;
        }

        ws_connection.m_PendingReaderFullPacketLen = full_data_len;
        ws_connection.m_PendingReader = reader;

        ws_connection.m_State = StormSocketServerConnectionWebsocketState::HandleContinuations;
      }

      if (ws_connection.m_State == StormSocketServerConnectionWebsocketState::HandleContinuations)
      {
        // Handle continuations
        StormWebsocketMessageReader reader = ws_connection.m_PendingReader;

        if (reader.m_DataType == StormSocketWebsocketDataType::Binary ||
          reader.m_DataType == StormSocketWebsocketDataType::Text ||
          reader.m_DataType == StormSocketWebsocketDataType::Continuation)
        {
          if (ws_connection.m_ReaderValid)
          {
            if (m_ContinuationMode == StormSocketContinuationMode::WaitForCompletion)
            {
              // Send this to the main thread
              StormSocketEventInfo data_message;
              data_message.GetWebsocketReader() = ws_connection.m_InitialReader;
              data_message.ConnectionId = connection_id;
              data_message.Type = StormSocketEventType::Data;
              data_message.RemoteIP = connection.m_RemoteIP;
              data_message.RemotePort = connection.m_RemotePort;

              if (m_EventQueue.Enqueue(data_message) == false)
              {
                return false;
              }

              if (m_EventSemaphore)
              {
                m_EventSemaphore->Release();
              }

              connection.m_PacketsRecved.fetch_add(1);
              ws_connection.m_InitialReader = reader;
              ws_connection.m_LastReader = reader;
            }
            else
            {
              ws_connection.m_LastReader.SetNextBlock(reader);
              ws_connection.m_LastReader = reader;

              if (m_ContinuationMode == StormSocketContinuationMode::Combine)
              {
                ws_connection.m_InitialReader.AddLength(reader.m_FullDataLen);
              }
            }
          }
          else
          {
            ws_connection.m_ReaderValid = true;
            ws_connection.m_InitialReader = reader;
            ws_connection.m_LastReader = reader;
          }
        }

        ws_connection.m_State = StormSocketServerConnectionWebsocketState::HandleIncomingPacket;
      }

      if (ws_connection.m_State == StormSocketServerConnectionWebsocketState::HandleIncomingPacket)
      {
        StormWebsocketMessageReader reader = ws_connection.m_PendingReader;
        if (reader.m_DataType == StormSocketWebsocketDataType::Ping)
        {
          // Reply with a pong
          StormWebsocketMessageWriter writer = CreateOutgoingPacket(StormWebsocketOp::Pong, true);

          // Copy data from the recv'd message to the output message
          int length = (int)reader.m_FullDataLen;
          while (length > 0)
          {
            if (length >= 4)
            {
              int val = reader.ReadInt32();
              writer.WriteInt32(val);
              length -= 4;
            }
            else
            {
              uint8_t val = reader.ReadByte();
              writer.WriteByte(val);
              length -= 1;
            }
          }

          // Release the reader
          if (ws_connection.m_ReaderValid)
          {
            // If we're in the middle of a continuation, this data has to be tagged onto the existing reader
            ws_connection.m_InitialReader.m_PacketDataLen += reader.m_PacketDataLen;
            reader.FreeChain();
          }
          else
          {
            FreeIncomingPacket(reader);
          }

          // Send the message
          writer.CreateHeaderAndApplyMask((int)StormWebsocketOp::Pong, true, rand());

          ws_connection.m_PendingWriter = writer;
          ws_connection.m_State = StormSocketServerConnectionWebsocketState::SendPong;

          // Advance past this packet to check if another packet is in the buffer
          m_Backend->DiscardParserData(connection_id, ws_connection.m_PendingReaderFullPacketLen);
        }
        else if (reader.m_DataType == StormSocketWebsocketDataType::Pong)
        {
          // Send this to the main thread
          StormSocketEventInfo data_message;
          data_message.GetWebsocketReader() = reader;
          data_message.ConnectionId = connection_id;
          data_message.Type = StormSocketEventType::Data;
          data_message.RemoteIP = connection.m_RemoteIP;
          data_message.RemotePort = connection.m_RemotePort;

          if (m_EventQueue.Enqueue(data_message) == false)
          {
            return false;
          }

          if (m_EventSemaphore)
          {
            m_EventSemaphore->Release();
          }

          // Advance past this packet to check if another packet is in the buffer
          m_Backend->DiscardParserData(connection_id, ws_connection.m_PendingReaderFullPacketLen);
          ws_connection.m_State = StormSocketServerConnectionWebsocketState::ReadHeaderAndApplyMask;
        }
        else
        {
          reader = ws_connection.m_InitialReader;
          if (reader.m_FinalInSequence || m_ContinuationMode == StormSocketContinuationMode::DeliverImmediately)
          {
            if (m_ContinuationMode == StormSocketContinuationMode::Combine)
            {
              ws_connection.m_InitialReader.m_FinalInSequence = true;
            }

            // Send this to the main thread
            StormSocketEventInfo data_message;
            data_message.GetWebsocketReader() = ws_connection.m_InitialReader;
            data_message.ConnectionId = connection_id;
            data_message.Type = StormSocketEventType::Data;
            data_message.RemoteIP = connection.m_RemoteIP;
            data_message.RemotePort = connection.m_RemotePort;

            if (m_EventQueue.Enqueue(data_message) == false)
            {
              return false;
            }

            if (m_EventSemaphore)
            {
              m_EventSemaphore->Release();
            }

            connection.m_PacketsRecved.fetch_add(1);
            ws_connection.m_ReaderValid = false;
          }

          m_Backend->DiscardParserData(connection_id, ws_connection.m_PendingReaderFullPacketLen);
          ws_connection.m_State = StormSocketServerConnectionWebsocketState::ReadHeaderAndApplyMask;
        }
      }

      if (ws_connection.m_State == StormSocketServerConnectionWebsocketState::SendPong)
      {
        StormMessageWriter writer = ws_connection.m_PendingWriter;
        if (SendPacketToConnection(writer, connection_id) == false)
        {
          return false;
        }

        ws_connection.m_State = StormSocketServerConnectionWebsocketState::ReadHeaderAndApplyMask;
      }
    }
  }

  void StormSocketFrontendWebsocketBase::CleanupWebsocketConnection(StormWebsocketConnectionBase & ws_connection)
  {
    if (ws_connection.m_ReaderValid)
    {
      ws_connection.m_InitialReader.FreeChain();
    }

    if (ws_connection.m_State == StormSocketServerConnectionWebsocketState::HandleContinuations)
    {
      FreeIncomingPacket(ws_connection.m_PendingReader);
    }

  }
}
