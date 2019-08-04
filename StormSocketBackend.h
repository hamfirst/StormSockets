#pragma once

#ifndef _INCLUDEOS
#include <asio/asio.hpp>
#else
#include <net/inet>
#endif

#include <map>
#include <optional>

#include "StormMemOps.h"
#include "StormFixedBlockAllocator.h"
#include "StormMessageQueue.h"
#include "StormSemaphore.h"
#include "StormMutex.h"
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
    int m_MaxConnections;

    StormFixedBlockAllocator m_Allocator;
    StormFixedBlockAllocator m_MessageSenders;
    StormFixedBlockAllocator m_MessageReaders;
    StormFixedBlockAllocator m_PendingSendBlocks;

    std::unique_ptr<StormSocketConnectionBase[]> m_Connections;
#ifndef _INCLUDEOS

    std::unique_ptr<std::optional<asio::steady_timer>[]> m_Timeouts;
  
    asio::io_service m_IOService;
    asio::ip::tcp::resolver m_Resolver;

    std::unique_ptr<std::optional<asio::ip::tcp::socket>[]> m_ClientSockets;

    std::unique_ptr<std::thread[]> m_IOThreads;
    std::unique_ptr<std::thread[]> m_SendThreads;

    std::unique_ptr<StormSemaphore[]> m_SendThreadSemaphores;
    std::unique_ptr<StormMessageMegaQueue<StormSocketIOOperation>[]> m_SendQueue;
    std::unique_ptr<StormMessageMegaContainer<StormSocketIOOperation>[]> m_SendQueueArray;
    std::unique_ptr<StormGenIndex[]> m_SendQueueIncdices;

    StormMessageQueue<StormSocketConnectionId> m_ClosingConnectionQueue;
    std::thread m_CloseConnectionThread;
    StormSemaphore m_CloseConnectionSemaphore;

    int m_NumSendThreads;
    int m_NumIOThreads;

    StormSemaphore m_IOResetSemaphore;
    std::atomic_int m_IOResetCount;

    static const int kBufferSetCount = 8;
    typedef std::array<asio::const_buffer, 8> SendBuffer;

#else

    std::unique_ptr<std::optional<id_t>[]> m_Timeouts;
    std::unique_ptr<std::optional<net::tcp::Connection_ptr>[]> m_ClientSockets;

#endif

    std::unique_ptr<StormMessageMegaQueue<StormMessageWriter>[]> m_OutputQueue;
    std::unique_ptr<StormMessageMegaContainer<StormMessageWriter>[]> m_OutputQueueArray;
    std::unique_ptr<StormGenIndex[]> m_OutputQueueIncdices;

    int m_FixedBlockSize;
    int m_HandshakeTimeout;
    bool m_ThreadStopRequested;

    static const std::size_t InvalidSocketId = -1;

    struct AcceptorData
    {
      StormSocketFrontend * m_Frontend;

#ifndef _INCLUDEOS      
      asio::ip::tcp::acceptor m_Acceptor;

      asio::ip::tcp::socket m_AcceptSocket;
      asio::ip::tcp::endpoint m_AcceptEndpoint;
#else
      net::tcp::Listener * m_Listener;
#endif
    };

    StormMutex m_AcceptorLock;
    std::map<StormSocketBackendAcceptorId, AcceptorData> m_Acceptors;
    StormSocketBackendAcceptorId m_NextAcceptorId;

    std::vector<Certificate> m_Certificates;

  public:

    StormSocketBackend(const StormSocketInitSettings & settings);
    virtual ~StormSocketBackend();

    std::vector<std::size_t> GetMallocReport();
    void MemoryAudit();
    void PrintConnections();
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

#ifndef _INCLUDEOS
    void PrepareToAccept(StormSocketBackendAcceptorId acceptor_id);
    void AcceptNewConnection(const asio::error_code& error, StormSocketBackendAcceptorId acceptor_id);
#endif

    void BootstrapConnection(StormSocketConnectionId connection_id, StormSocketConnectionBase & connection, void * ssl_config_ptr);
    void PrepareToConnect(StormSocketConnectionId id, uint32_t addr, uint16_t port);

    void FinalizeConnectToHost(StormSocketConnectionId id);
    void ConnectFailed(StormSocketConnectionId id);

    void ProcessNewData(StormSocketConnectionId connection_id, bool error, std::size_t bytes_received);
    bool ProcessReceivedData(StormSocketConnectionId connection_id, bool recv_failure);
    void PrepareToRecv(StormSocketConnectionId connection_id);
    void TryProcessReceivedData(StormSocketConnectionId connection_id, bool recv_failure);

#ifndef _INCLUDEOS
    void IOThreadMain();
    void SendThreadMain(int thread_index);
#endif
    void TransmitConnectionPackets(StormSocketConnectionId connection_id);

    StormFixedBlockHandle ReleasePendingSendBlock(StormFixedBlockHandle send_block_handle, StormPendingSendBlock * send_block);
    void ReleaseSendQueue(StormSocketConnectionId connection_id, int connection_gen);
    StormMessageWriter EncryptWriter(StormSocketConnectionId connection_id, StormMessageWriter & writer);

#ifndef _INCLUDEOS
    void CloseSocketThread();
#endif
    void QueueCloseSocket(StormSocketConnectionId id);
    void CloseSocket(StormSocketConnectionId id);

    void FreeConnectionResources(StormSocketConnectionId id);
  };
}

