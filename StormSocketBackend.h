#pragma once

#include <asio/asio.hpp>
#include <optional/optional.hpp>

#include <map>

#include "StormMemOps.h"
#include "StormFixedBlockAllocator.h"
#include "StormMessageQueue.h"
#include "StormSemaphore.h"
#include "StormSocketConnectionId.h"
#include "StormMessageWriter.h"
#include "StormHttpRequestWriter.h"
#include "StormHttpResponseWriter.h"
#include "StormWebsocketMessageReader.h"
#include "StormSocketServerTypes.h"
#include "StormSocketIOOperation.h"
#include "StormSocketFrontend.h"

#ifndef DISABLE_MBED
#include "mbedtls/ssl.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#endif


namespace StormSockets
{
  using StormSocketBackendAcceptorId = int;

  struct StormPendingSendBlock;

  struct Certificate
  {
    std::unique_ptr<uint8_t[]> m_Data;
    std::size_t m_Length;
  };

  class StormSocketBackend
  {
    StormFixedBlockAllocator m_Allocator;
    StormFixedBlockAllocator m_MessageSenders;
    StormFixedBlockAllocator m_MessageReaders;
    StormFixedBlockAllocator m_PendingSendBlocks;

    std::unique_ptr<StormSocketConnectionBase[]> m_Connections;
    std::unique_ptr<std::experimental::optional<asio::steady_timer>[]> m_Timeouts;

    asio::io_service m_IOService;
    asio::ip::tcp::resolver m_Resolver;

    std::unique_ptr<std::experimental::optional<asio::ip::tcp::socket>[]> m_ClientSockets;

    std::unique_ptr<std::thread[]> m_IOThreads;
    std::unique_ptr<std::thread[]> m_SendThreads;

    std::unique_ptr<StormMessageMegaQueue<StormMessageWriter>[]> m_OutputQueue;
    std::unique_ptr<StormMessageMegaContainer<StormMessageWriter>[]> m_OutputQueueArray;
    std::unique_ptr<StormGenIndex[]> m_OutputQueueIncdices;

    std::unique_ptr<StormSemaphore[]> m_SendThreadSemaphores;
    std::unique_ptr<StormMessageMegaQueue<StormSocketIOOperation>[]> m_SendQueue;
    std::unique_ptr<StormMessageMegaContainer<StormSocketIOOperation>[]> m_SendQueueArray;
    std::unique_ptr<StormGenIndex[]> m_SendQueueIncdices;

    StormMessageQueue<StormSocketConnectionId> m_ClosingConnectionQueue;
    std::thread m_CloseConnectionThread;
    StormSemaphore m_CloseConnectionSemaphore;

    int m_MaxConnections;
    int m_NumSendThreads;
    int m_NumIOThreads;

    StormSemaphore m_IOResetSemaphore;
    std::atomic_int m_IOResetCount;

    int m_FixedBlockSize;
    int m_HandshakeTimeout;
    bool m_ThreadStopRequested;

    static const int kBufferSetCount = 8;
    typedef std::array<asio::const_buffer, 8> SendBuffer;
    static const std::size_t InvalidSocketId = -1;

    struct AcceptorData
    {
      StormSocketFrontend * m_Frontend;
      asio::ip::tcp::acceptor m_Acceptor;

      asio::ip::tcp::socket m_AcceptSocket;
      asio::ip::tcp::endpoint m_AcceptEndpoint;
    };

    std::mutex m_AcceptorLock;
    std::map<StormSocketBackendAcceptorId, AcceptorData> m_Acceptors;
    StormSocketBackendAcceptorId m_NextAcceptorId;

    std::vector<Certificate> m_Certificates;

  public:

    StormSocketBackend(const StormSocketInitSettings & settings);
    virtual ~StormSocketBackend();

    std::vector<std::size_t> GetMallocReport();
    void MemoryAudit();
    std::vector<Certificate> & GetCertificates();

    StormSocketBackendAcceptorId InitAcceptor(StormSocketFrontend * frontend, const StormSocketListenData & init_data);
    void DestroyAcceptor(StormSocketBackendAcceptorId id);

    StormSocketConnectionId RequestConnect(StormSocketFrontend * frontend, const char * ip_addr, int port, const void * init_data);

    void RequestStop() { m_ThreadStopRequested = true; }

    int GetFixedBlockSize() { return m_FixedBlockSize; }
    StormFixedBlockAllocator & GetAllocator() { return m_Allocator; }
    StormFixedBlockAllocator & GetMessageSenders() { return m_MessageSenders; }
    StormFixedBlockAllocator & GetMessageReaders() { return m_MessageReaders; }

    StormSocketConnectionBase & GetConnection(int index);

    StormMessageWriter CreateWriter(bool is_encrypted = false);
    StormHttpRequestWriter CreateHttpRequestWriter(const char * method, const char * uri, const char * host);
    StormHttpResponseWriter CreateHttpResponseWriter(int response_code, const char * response_phrase);
    void ReferenceOutgoingHttpRequest(StormHttpRequestWriter & writer);
    void ReferenceOutgoingHttpResponse(StormHttpResponseWriter & writer);
    void FreeOutgoingHttpRequest(StormHttpRequestWriter & writer);
    void FreeOutgoingHttpResponse(StormHttpResponseWriter & writer);

    bool SendPacketToConnection(StormMessageWriter & writer, StormSocketConnectionId id);
    void SendPacketToConnectionBlocking(StormMessageWriter & writer, StormSocketConnectionId id);
    void SendHttpRequestToConnection(StormHttpRequestWriter & writer, StormSocketConnectionId id);
    void SendHttpResponseToConnection(StormHttpResponseWriter & writer, StormSocketConnectionId id);
    void SendHttpToConnection(StormMessageWriter & header_writer, StormMessageWriter & body_writer, StormSocketConnectionId id);
    void FreeOutgoingPacket(StormMessageWriter & writer);
    void FinalizeConnection(StormSocketConnectionId id);
    void ForceDisconnect(StormSocketConnectionId id);
    bool ConnectionIdValid(StormSocketConnectionId id);

    void DiscardParserData(StormSocketConnectionId connection_id, int amount);
    void DiscardReaderData(StormSocketConnectionId connection_id, int amount);

    bool ReservePacketSlot(StormSocketConnectionId id, int amount = 1);
    void ReleasePacketSlot(StormSocketConnectionId id, int amount = 1);

    void ReleaseOutgoingPacket(StormMessageWriter & writer);
    void SetSocketDisconnected(StormSocketConnectionId id);

    void SignalCloseThread(StormSocketConnectionId id);
    void SetDisconnectFlag(StormSocketConnectionId id, StormSocketDisconnectFlags::Index flags);
    bool CheckDisconnectFlags(StormSocketConnectionId id, StormSocketDisconnectFlags::Index new_flags);
    void SetHandshakeComplete(StormSocketConnectionId id);

    bool QueueOutgoingPacket(StormMessageWriter & writer, StormSocketConnectionId id);
    void SignalOutgoingSocket(StormSocketConnectionId id, StormSocketIOOperationType::Index type, std::size_t size = 0);

  private:

    StormSocketConnectionId AllocateConnection(StormSocketFrontend * frontend, uint32_t remote_ip, uint16_t remote_port, bool for_connect, const void * init_data);
    void FreeConnectionSlot(StormSocketConnectionId id);

    void PrepareToAccept(StormSocketBackendAcceptorId acceptor_id);
    void AcceptNewConnection(const asio::error_code& error, StormSocketBackendAcceptorId acceptor_id);

    void PrepareToConnect(StormSocketConnectionId id, asio::ip::tcp::endpoint endpoint);
    void FinalizeConnectToHost(StormSocketConnectionId id);
    void ConnectFailed(StormSocketConnectionId id);

    void ProcessNewData(StormSocketConnectionId connection_id, const asio::error_code & error, std::size_t bytes_received);
    bool ProcessReceivedData(StormSocketConnectionId connection_id, bool recv_failure);
    void PrepareToRecv(StormSocketConnectionId connection_id);
    void TryProcessReceivedData(StormSocketConnectionId connection_id, bool recv_failure);

    void IOThreadMain();
    void TransmitConnectionPackets(StormSocketConnectionId connection_id);

    void SendThreadMain(int thread_index);

    StormFixedBlockHandle ReleasePendingSendBlock(StormFixedBlockHandle send_block_handle, StormPendingSendBlock * send_block);
    void ReleaseSendQueue(StormSocketConnectionId connection_id, int connection_gen);
    StormMessageWriter EncryptWriter(StormSocketConnectionId connection_id, StormMessageWriter & writer);

    void CloseSocketThread();
    void QueueCloseSocket(StormSocketConnectionId id);
    void CloseSocket(StormSocketConnectionId id);
    void FreeConnectionResources(StormSocketConnectionId id);
  };
}

