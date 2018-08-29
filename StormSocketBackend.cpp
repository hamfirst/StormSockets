
#include "StormSocketBackend.h"
#include "StormSocketLog.h"

#include <fstream>

#ifndef DISABLE_MBED
#include "mbedtls/error.h"
#include "mbedtls/debug.h"
#include "mbedtls/threading.h"
#endif

#ifndef DISABLE_MBED

#ifdef _WINDOWS
#include <sspi.h>
#include <schnlsp.h>
#include <ntsecapi.h>

#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Secur32.lib")
#endif
#endif

#ifdef _LINUX
#include <cstdio>

#include <dirent.h>
#endif

#ifdef _WINDOWS
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "mswsock.lib")
#endif

#include <stdexcept>

std::atomic_int g_StormSocketsNumTlsConnections = {};

static struct StormMbedTlsInit
{
  StormMbedTlsInit()
  {
    mbedtls_threading_set_alt(
      [](mbedtls_threading_mutex_t * mtx) { *mtx = new std::mutex(); },
      [](mbedtls_threading_mutex_t * mtx) { auto m = (std::mutex *)*mtx; delete m; },
      [](mbedtls_threading_mutex_t * mtx) { auto m = (std::mutex *)*mtx; m->lock(); return 0; },
      [](mbedtls_threading_mutex_t * mtx) { auto m = (std::mutex *)*mtx; m->unlock(); return 0; });
  }

  void Reference()
  {

  }

} s_StormMbedTlsInit;

namespace StormSockets
{
  struct StormPendingSendBlock
  {
    void * m_DataStart;
    int m_DataLen;

    StormFixedBlockHandle m_StartBlock;
    StormFixedBlockHandle m_PacketHandle;
    std::atomic_int * m_RefCount;
  };

  StormSocketBackend::StormSocketBackend(const StormSocketInitSettings & settings) :
    m_Allocator(settings.HeapSize, settings.BlockSize, false),
    m_MessageReaders(settings.MaxPendingOutgoingPacketsPerConnection * sizeof(StormMessageReaderData) * settings.MaxConnections, sizeof(StormMessageReaderData), false),
    m_MessageSenders(settings.MaxPendingOutgoingPacketsPerConnection * sizeof(StormMessageWriterData) * settings.MaxConnections, sizeof(StormMessageWriterData), false),
    m_PendingSendBlocks(settings.MaxPendingSendBlocks, sizeof(StormPendingSendBlock), false),
    m_ClosingConnectionQueue(settings.MaxConnections),
    m_Resolver(m_IOService)
  {
    s_StormMbedTlsInit.Reference();

    m_NextAcceptorId = 0;
    m_MaxConnections = settings.MaxConnections;
    m_ThreadStopRequested = false;
    m_NumSendThreads = settings.NumSendThreads;
    m_NumIOThreads = settings.NumIOThreads;
    m_HandshakeTimeout = settings.HandshakeTimeout;

    m_Connections = std::make_unique<StormSocketConnectionBase[]>(settings.MaxConnections);
    m_Timeouts = std::make_unique<std::experimental::optional<asio::steady_timer>[]>(settings.MaxConnections);

    m_SendThreadSemaphores = std::make_unique<StormSemaphore[]>(settings.NumSendThreads);
    m_SendQueue = std::make_unique<StormMessageMegaQueue<StormSocketIOOperation>[]>(settings.NumSendThreads);
    m_SendQueueArray = std::make_unique<StormMessageMegaContainer<StormSocketIOOperation>[]>(settings.NumSendThreads * settings.MaxSendQueueElements);
    m_SendQueueIncdices = std::make_unique<StormGenIndex[]>(settings.NumSendThreads * settings.MaxSendQueueElements);

    m_OutputQueue = std::make_unique<StormMessageMegaQueue<StormMessageWriter>[]>(settings.MaxConnections);
    m_OutputQueueArray = std::make_unique<StormMessageMegaContainer<StormMessageWriter>[]>(settings.MaxConnections * settings.MaxPendingOutgoingPacketsPerConnection);
    m_OutputQueueIncdices = std::make_unique<StormGenIndex[]>(settings.MaxConnections * settings.MaxPendingOutgoingPacketsPerConnection);

    m_CloseConnectionSemaphore.Init(settings.MaxConnections);
    m_CloseConnectionThread = std::thread(&StormSocketBackend::CloseSocketThread, this);

    m_FixedBlockSize = settings.BlockSize;

    for (int index = 0; index < settings.MaxConnections; index++)
    {
      m_OutputQueue[index].Init(m_OutputQueueIncdices.get(), m_OutputQueueArray.get(),
        index * settings.MaxPendingOutgoingPacketsPerConnection, settings.MaxPendingOutgoingPacketsPerConnection);
    }

    for (int index = 0; index < settings.NumSendThreads; index++)
    {
      m_SendQueue[index].Init(m_SendQueueIncdices.get(), m_SendQueueArray.get(),
        index * settings.MaxSendQueueElements, settings.MaxSendQueueElements);

      int semaphore_max = settings.MaxSendQueueElements + (settings.MaxConnections * settings.MaxPendingOutgoingPacketsPerConnection) / settings.NumSendThreads;
      m_SendThreadSemaphores[index].Init(semaphore_max * 2);
    }

    m_ClientSockets = std::make_unique<std::experimental::optional<asio::ip::tcp::socket>[]>(settings.MaxConnections);

    // Start the io threads
    m_IOResetCount = 0;
    m_IOResetSemaphore.Init(INT_MAX);

    m_IOThreads = std::make_unique<std::thread[]>(m_NumIOThreads);
    for (int index = 0; index < m_NumIOThreads; index++)
    {
      m_IOThreads[index] = std::thread(&StormSocketBackend::IOThreadMain, this);
    }

    m_SendThreads = std::make_unique<std::thread[]>(m_NumSendThreads);
    for (int index = 0; index < m_NumSendThreads; index++)
    {
      m_SendThreads[index] = std::thread(&StormSocketBackend::SendThreadMain, this, index);
    }

    std::vector<Certificate> certs;

#ifdef _WINDOWS

    auto cert_store = CertOpenSystemStore(NULL, TEXT("ROOT"));
    PCCERT_CONTEXT cert_context = nullptr;

    while ((cert_context = CertEnumCertificatesInStore(cert_store, cert_context)) != nullptr)
    {
      if ((cert_context->dwCertEncodingType & X509_ASN_ENCODING) != 0)
      {
        Certificate cert;
        cert.m_Data = std::make_unique<uint8_t[]>(cert_context->cbCertEncoded + 1);
        cert.m_Length = cert_context->cbCertEncoded + 1;

        memcpy(cert.m_Data.get(), cert_context->pbCertEncoded, cert_context->cbCertEncoded);
        cert.m_Data[cert_context->cbCertEncoded] = 0;

        certs.emplace_back(std::move(cert));
      }
    }

    CertCloseStore(cert_store, 0);
#endif

#ifdef _LINUX
    auto dir = opendir("/etc/ssl/certs");
    if (dir != nullptr)
    {
      while (true)
      {
        auto ent = readdir(dir);
        if (ent == nullptr)
        {
          closedir(dir);
          break;
        }

        if (ent->d_type == DT_LNK || ent->d_type == DT_REG || ent->d_type == DT_UNKNOWN)
        {
          if (strstr(ent->d_name, ".crt"))
          {
            std::string crt_filename = std::string("/etc/ssl/certs/") + ent->d_name;

            auto fp = fopen(crt_filename.c_str(), "rb");
            if (fp == nullptr)
            {
              continue;
            }

            fseek(fp, 0, SEEK_END);
            auto len = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            Certificate cert;
            cert.m_Data = std::make_unique<uint8_t[]>(len + 1);
            cert.m_Length = len + 1;

            fread(cert.m_Data.get(), 1, len, fp);
            cert.m_Data[len] = 0;

            certs.emplace_back(std::move(cert));

            fclose(fp);
          }
        }
      }
    }
#endif

    m_Certificates = std::move(certs);
  }

  StormSocketBackend::~StormSocketBackend()
  {
    std::unique_lock<std::mutex> acceptor_lock(m_AcceptorLock);
    m_Acceptors.clear();
    acceptor_lock.unlock();

    m_ThreadStopRequested = true;

    for (int index = 0; index < m_NumIOThreads; index++)
    {
      m_IOThreads[index].join();
    }

    for (int index = 0; index < m_NumSendThreads; index++)
    {
      m_SendThreadSemaphores[index].Release();
      m_SendThreads[index].join();
    }

    m_CloseConnectionSemaphore.Release();
    m_CloseConnectionThread.join();

    for (int index = 0; index < m_MaxConnections; index++)
    {
      auto & connection = GetConnection(index);
      if (connection.m_Used.test_and_set())
      {
        auto connection_id = StormSocketConnectionId(index, connection.m_SlotGen);
        FreeConnectionResources(connection_id);
      }
    }
  }

  std::vector<std::size_t> StormSocketBackend::GetMallocReport()
  {
    std::vector<std::size_t> vec;
    vec.push_back(m_Allocator.GetOutstandingMallocs());
    vec.push_back(m_MessageSenders.GetOutstandingMallocs());
    vec.push_back(m_MessageReaders.GetOutstandingMallocs());
    vec.push_back(m_PendingSendBlocks.GetOutstandingMallocs());
    return vec;
  }
  
  void StormSocketBackend::MemoryAudit()
  {
    auto malloc_report = GetMallocReport();
    printf("Main allocator size: %d\n", (int)malloc_report[0]);
    printf("Sender allocator size: %d\n", (int)malloc_report[1]);
    printf("Reader allocator size: %d\n", (int)malloc_report[2]);
    printf("Pending allocator size: %d\n", (int)malloc_report[3]);
    printf("Acceptor size: %d\n", (int)m_Acceptors.size());

    int num_connections = 0;
    for(int index = 0; index < m_MaxConnections; ++index)
    {
      if(m_Connections[index].m_Allocated)
      {
        num_connections++;
      }
    }

    printf("Allocated connections: %d\n", num_connections);
    printf("Webrtc SSL connections: %d\n", (int)g_StormSocketsNumTlsConnections);
  }

  std::vector<Certificate> & StormSocketBackend::GetCertificates()
  {
    return m_Certificates;
  }

  StormSocketBackendAcceptorId StormSocketBackend::InitAcceptor(StormSocketFrontend * frontend, const StormSocketListenData & init_data)
  {
    std::unique_lock<std::mutex> guard(m_AcceptorLock);

    AcceptorData new_acceptor = { frontend,
      asio::ip::tcp::acceptor(m_IOService),
      asio::ip::tcp::socket(m_IOService)
    };

    int acceptor_id = m_NextAcceptorId;
    m_NextAcceptorId++;
    auto acceptor_pair = m_Acceptors.emplace(std::make_pair(acceptor_id, std::move(new_acceptor)));
    auto & acceptor = acceptor_pair.first->second;

    asio::ip::tcp::endpoint endpoint(asio::ip::address_v4::from_string(init_data.LocalInterface), init_data.Port);
    acceptor.m_Acceptor.open(asio::ip::tcp::v4());
    acceptor.m_Acceptor.set_option(asio::ip::tcp::no_delay(true));
    acceptor.m_Acceptor.set_option(asio::socket_base::reuse_address(true));
    acceptor.m_Acceptor.bind(endpoint);
    acceptor.m_Acceptor.listen();

    guard.unlock();

    PrepareToAccept(acceptor_id);

    return acceptor_id;
  }

  void StormSocketBackend::DestroyAcceptor(StormSocketBackendAcceptorId id)
  {
    std::lock_guard<std::mutex> guard(m_AcceptorLock);
    auto acceptor_itr = m_Acceptors.find(id);

    if (acceptor_itr != m_Acceptors.end())
    {
      m_Acceptors.erase(acceptor_itr);
    }
  }

  StormSocketConnectionId StormSocketBackend::RequestConnect(StormSocketFrontend * frontend, const char * ip_addr, int port, const void * init_data)
  {
    asio::ip::tcp::socket socket(m_IOService);
    asio::error_code ec;
    socket.open(asio::ip::tcp::v4(), ec);

    if(ec)
    {
      StormSocketLog("Could not create new client socket\n");
      return StormSocketConnectionId::InvalidConnectionId;
    }

    socket.set_option(asio::ip::tcp::no_delay(true), ec);

    auto connection_id = AllocateConnection(frontend, 0, port, true, init_data);

    if (connection_id == StormSocketConnectionId::InvalidConnectionId)
    {
      StormSocketLog("Could not allocate connection id\n");

      socket.close();
      return StormSocketConnectionId::InvalidConnectionId;
    }

    m_ClientSockets[connection_id].emplace(std::move(socket));
    auto numerical_addr = asio::ip::address_v4::from_string(ip_addr, ec);

    if (!ec)
    {
      PrepareToConnect(connection_id, asio::ip::tcp::endpoint(numerical_addr, port));
    }
    else
    {
      asio::ip::tcp::resolver::query resolver_query(ip_addr, std::to_string(port));

      auto resolver_callback = [this, connection_id, port](asio::error_code ec, asio::ip::tcp::resolver::iterator itr)
      {
        if (!ec)
        {
          while (itr != asio::ip::tcp::resolver::iterator())
          {
            asio::ip::tcp::endpoint ep = *itr;

            if (ep.protocol() == ep.protocol().v4())
            {
              PrepareToConnect(connection_id, asio::ip::tcp::endpoint(ep.address(), port));
              return;
            }

            ++itr;
          }
                
          StormSocketLog("Resolve failed\n");
          ConnectFailed(connection_id);
        }
        else
        {
          StormSocketLog("Resolve failed\n");
          ConnectFailed(connection_id);
        }
      };

      m_Resolver.async_resolve(resolver_query, resolver_callback);
    }

    return connection_id;
  }

  bool StormSocketBackend::QueueOutgoingPacket(StormMessageWriter & writer, StormSocketConnectionId id)
  {
    return m_OutputQueue[id].Enqueue(writer, id.GetGen(), m_OutputQueueIncdices.get(), m_OutputQueueArray.get());
  }

  StormSocketConnectionBase & StormSocketBackend::GetConnection(int index)
  {
    if (index >= m_MaxConnections)
    {
      throw std::runtime_error("Invalid connection id");
    }

    return m_Connections[index];
  }

  StormMessageWriter StormSocketBackend::CreateWriter(bool is_encrypted)
  {
    uint64_t prof = Profiling::StartProfiler();
    StormMessageWriter writer;
    writer.Init(&m_Allocator, &m_MessageSenders, is_encrypted, 0, 0);
    Profiling::EndProfiler(prof, ProfilerCategory::kCreatePacket);
    return writer;
  }

  StormHttpRequestWriter StormSocketBackend::CreateHttpRequestWriter(const char * method, const char * uri, const char * host)
  {
    auto header_writer = CreateWriter();
    auto body_writer = CreateWriter();

    StormHttpRequestWriter writer(method, uri, host, header_writer, body_writer);
    return writer;
  }

  StormHttpResponseWriter StormSocketBackend::CreateHttpResponseWriter(int response_code, const char * response_phrase)
  {
    auto header_writer = CreateWriter();
    auto body_writer = CreateWriter();

    StormHttpResponseWriter writer(response_code, response_phrase, header_writer, body_writer);
    return writer;
  }

  void StormSocketBackend::ReferenceOutgoingHttpRequest(StormHttpRequestWriter & writer)
  {
    writer.m_HeaderWriter.m_PacketInfo->m_RefCount.fetch_add(1);
    writer.m_BodyWriter.m_PacketInfo->m_RefCount.fetch_add(1);
  }

  void StormSocketBackend::ReferenceOutgoingHttpResponse(StormHttpResponseWriter & writer)
  {
    writer.m_HeaderWriter.m_PacketInfo->m_RefCount.fetch_add(1);
    writer.m_BodyWriter.m_PacketInfo->m_RefCount.fetch_add(1);
  }

  void StormSocketBackend::FreeOutgoingHttpRequest(StormHttpRequestWriter & writer)
  {
    FreeOutgoingPacket(writer.m_HeaderWriter);
    FreeOutgoingPacket(writer.m_BodyWriter);
  }

  void StormSocketBackend::FreeOutgoingHttpResponse(StormHttpResponseWriter & writer)
  {
    FreeOutgoingPacket(writer.m_HeaderWriter);
    FreeOutgoingPacket(writer.m_BodyWriter);
  }

  bool StormSocketBackend::SendPacketToConnection(StormMessageWriter & writer, StormSocketConnectionId id)
  {
    if (writer.m_PacketInfo->m_TotalLength == 0)
    {
      return false;
    }

    auto & connection = GetConnection(id);
    if (connection.m_SlotGen != id.GetGen())
    {
      return false;
    }

    if (!ReservePacketSlot(id))
    {
      return false;
    }

    writer.m_PacketInfo->m_RefCount.fetch_add(1);
    if (QueueOutgoingPacket(writer, id) == false)
    {
      ReleasePacketSlot(id);
      writer.m_PacketInfo->m_RefCount.fetch_sub(1);
      return false;
    }

    connection.m_PacketsSent.fetch_add(1);
    SignalOutgoingSocket(id, StormSocketIOOperationType::QueuePacket);
    return true;
  }

  void StormSocketBackend::SendPacketToConnectionBlocking(StormMessageWriter & writer, StormSocketConnectionId id)
  {
    while (!ReservePacketSlot(id))
    {
      std::this_thread::yield();
    }

    auto & connection = GetConnection(id);

    writer.m_PacketInfo->m_RefCount.fetch_add(1);
    while (QueueOutgoingPacket(writer, id) == false)
    {
      if (connection.m_SlotGen != id.GetGen())
      {
        ReleasePacketSlot(id);
        writer.m_PacketInfo->m_RefCount.fetch_sub(1);
        return;
      }

      if ((connection.m_DisconnectFlags & StormSocketDisconnectFlags::kTerminateFlags) != 0)
      {
        ReleasePacketSlot(id);
        writer.m_PacketInfo->m_RefCount.fetch_sub(1);
        return;
      }

      std::this_thread::yield();
    }

    SignalOutgoingSocket(id, StormSocketIOOperationType::QueuePacket);
  }

  void StormSocketBackend::SendHttpRequestToConnection(StormHttpRequestWriter & writer, StormSocketConnectionId id)
  {    
    SendHttpToConnection(writer.m_HeaderWriter, writer.m_BodyWriter, id);
  }

  void StormSocketBackend::SendHttpResponseToConnection(StormHttpResponseWriter & writer, StormSocketConnectionId id)
  {
    SendHttpToConnection(writer.m_HeaderWriter, writer.m_BodyWriter, id);
  }

  void StormSocketBackend::SendHttpToConnection(StormMessageWriter & header_writer, StormMessageWriter & body_writer, StormSocketConnectionId id)
  {
    if (body_writer.GetLength() == 0)
    {
      SendPacketToConnectionBlocking(header_writer, id);
      return;
    }

    auto & connection = GetConnection(id);
    if (connection.m_SlotGen != id.GetGen())
    {
      return;
    }

    if (!ReservePacketSlot(id, 2))
    {
      return;
    }

    header_writer.m_PacketInfo->m_RefCount.fetch_add(1);
    body_writer.m_PacketInfo->m_RefCount.fetch_add(1);
    while (QueueOutgoingPacket(header_writer, id) == false)
    {
      if (connection.m_SlotGen != id.GetGen())
      {
        ReleasePacketSlot(id, 2);
        header_writer.m_PacketInfo->m_RefCount.fetch_sub(1);
        body_writer.m_PacketInfo->m_RefCount.fetch_sub(1);
        return;
      }

      if ((connection.m_DisconnectFlags & StormSocketDisconnectFlags::kTerminateFlags) != 0)
      {
        ReleasePacketSlot(id, 2);
        header_writer.m_PacketInfo->m_RefCount.fetch_sub(1);
        body_writer.m_PacketInfo->m_RefCount.fetch_sub(1);
        return;
      }

      std::this_thread::yield();
    }

    while (QueueOutgoingPacket(body_writer, id) == false)
    {
      if (connection.m_SlotGen != id.GetGen())
      {
        ReleasePacketSlot(id);
        body_writer.m_PacketInfo->m_RefCount.fetch_sub(1);
        return;
      }

      if ((connection.m_DisconnectFlags & StormSocketDisconnectFlags::kTerminateFlags) != 0)
      {
        ReleasePacketSlot(id);
        body_writer.m_PacketInfo->m_RefCount.fetch_sub(1);
        return;
      }

      std::this_thread::yield();
    }

    connection.m_PacketsSent.fetch_add(2);

    int send_thread_index = id % m_NumSendThreads;

    StormSocketIOOperation op;
    op.m_ConnectionId = id;
    op.m_Type = StormSocketIOOperationType::QueuePacket;
    op.m_Size = 0;

    while (m_SendQueue[send_thread_index].Enqueue(op, 0, m_SendQueueIncdices.get(), m_SendQueueArray.get()) == false)
    {
      std::this_thread::yield();
    }

    while (m_SendQueue[send_thread_index].Enqueue(op, 0, m_SendQueueIncdices.get(), m_SendQueueArray.get()) == false)
    {
      std::this_thread::yield();
    }

    m_SendThreadSemaphores[send_thread_index].Release(2);
  }

  void StormSocketBackend::FreeOutgoingPacket(StormMessageWriter & writer)
  {
    if (writer.m_PacketInfo->m_RefCount.fetch_sub(1) == 1)
    {
      ReleaseOutgoingPacket(writer);
    }
  }

  void StormSocketBackend::FinalizeConnection(StormSocketConnectionId id)
  {
    SetDisconnectFlag(id, StormSocketDisconnectFlags::kMainThread);
  }

  void StormSocketBackend::ForceDisconnect(StormSocketConnectionId id)
  {
    SetDisconnectFlag(id, StormSocketDisconnectFlags::kLocalClose);
    SetDisconnectFlag(id, StormSocketDisconnectFlags::kSignalClose);
  }

  bool StormSocketBackend::ConnectionIdValid(StormSocketConnectionId id)
  {
    auto & connection = GetConnection(id);
    return id.GetGen() == connection.m_SlotGen;
  }

  void StormSocketBackend::DiscardParserData(StormSocketConnectionId connection_id, int amount)
  {
    auto & connection = GetConnection(connection_id);
    if (connection.m_UnparsedDataLength < amount)
    {
      throw std::runtime_error("Read buffer underflow");
    }

    connection.m_ParseOffset += amount;
    while (connection.m_ParseOffset >= m_FixedBlockSize)
    {
      connection.m_ParseBlock = m_Allocator.GetNextBlock(connection.m_ParseBlock);
      connection.m_ParseOffset -= m_FixedBlockSize;

      if (connection.m_ParseBlock == InvalidBlockHandle)
      {
        connection.m_ParseBlock = connection.m_RecvBuffer.m_BlockStart;
      }
    }

    connection.m_UnparsedDataLength.fetch_sub(amount);
  }

  void StormSocketBackend::DiscardReaderData(StormSocketConnectionId connection_id, int amount)
  {
    auto & connection = GetConnection(connection_id);
    connection.m_RecvBuffer.DiscardData(amount);
  }

  bool StormSocketBackend::ReservePacketSlot(StormSocketConnectionId id, int amount)
  {
    auto & connection = GetConnection(id);

    while (true)
    {
      int pending_packets = connection.m_PendingPackets;
      if (std::atomic_compare_exchange_weak(&connection.m_PendingPackets, &pending_packets, pending_packets + amount))
      {
        return true;
      }
    }
  }

  void StormSocketBackend::ReleasePacketSlot(StormSocketConnectionId id, int amount)
  {
    auto & connection = GetConnection(id);
    if (id.GetGen() == connection.m_SlotGen)
    {
      connection.m_PendingPackets.fetch_sub(amount);
    }
  }

  void StormSocketBackend::ReleaseOutgoingPacket(StormMessageWriter & writer)
  {
    StormFixedBlockHandle start_block = writer.m_PacketInfo->m_StartBlock;
    m_Allocator.FreeBlockChain(start_block, StormFixedBlockType::BlockMem);
    m_MessageSenders.FreeBlock(writer.m_PacketHandle, StormFixedBlockType::Sender);
  }

  void StormSocketBackend::SetSocketDisconnected(StormSocketConnectionId id)
  {
    auto & connection = GetConnection(id);

    while (true)
    {
      if (id.GetGen() != connection.m_SlotGen)
      {
        return;
      }

      StormSocketDisconnectFlags::Index cur_flags = (StormSocketDisconnectFlags::Index)connection.m_DisconnectFlags;
      if ((cur_flags & StormSocketDisconnectFlags::kSocket) != 0)
      {
        return;
      }

      int new_flags = cur_flags | StormSocketDisconnectFlags::kSocket | StormSocketDisconnectFlags::kLocalClose | StormSocketDisconnectFlags::kRemoteClose;
      if (std::atomic_compare_exchange_weak((std::atomic_int *)&connection.m_DisconnectFlags, (int *)&cur_flags, (int)new_flags))
      {
        if (connection.m_PendingSendBlockStart != InvalidBlockHandle)
        {
          //printf("bad\n");
        }

        // Tell the sending thread to flush the queue
        SignalOutgoingSocket(id, StormSocketIOOperationType::ClearQueue);

        connection.m_Frontend->QueueDisconnectEvent(id, connection.m_FrontendId);

        CheckDisconnectFlags(id, (StormSocketDisconnectFlags::Index)new_flags);
        return;
      }
    }
  }

  void StormSocketBackend::SignalCloseThread(StormSocketConnectionId id)
  {
    SetDisconnectFlag(id, StormSocketDisconnectFlags::kSignalClose);
  }

  void StormSocketBackend::SetDisconnectFlag(StormSocketConnectionId id, StormSocketDisconnectFlags::Index flags)
  {
    auto & connection = GetConnection(id);

    while (true)
    {
      if (id.GetGen() != connection.m_SlotGen)
      {
        return;
      }

      StormSocketDisconnectFlags::Index cur_flags = (StormSocketDisconnectFlags::Index)connection.m_DisconnectFlags;
      if ((cur_flags & flags) != 0)
      {
        return;
      }

      int new_flags = cur_flags | flags;
      if (std::atomic_compare_exchange_weak((std::atomic_int *)&connection.m_DisconnectFlags, (int *)&cur_flags, new_flags))
      {
        if (CheckDisconnectFlags(id, (StormSocketDisconnectFlags::Index)new_flags))
        {
          return;
        }

        if (flags == StormSocketDisconnectFlags::kLocalClose)
        {
          connection.m_Frontend->SendClosePacket(id, connection.m_FrontendId);
        }

        if ((flags == StormSocketDisconnectFlags::kLocalClose || flags == StormSocketDisconnectFlags::kRemoteClose) &&
          (new_flags & StormSocketDisconnectFlags::kSocket) == 0 &&
          (new_flags & StormSocketDisconnectFlags::kCloseFlags) == StormSocketDisconnectFlags::kCloseFlags)
        {
          SignalOutgoingSocket(id, StormSocketIOOperationType::Close);
        }

        if (flags == StormSocketDisconnectFlags::kSignalClose)
        {
          if (connection.m_PendingSendBlockStart != InvalidBlockHandle)
          {
            //printf("bad\n");
          }

          QueueCloseSocket(id);
          connection.m_FailedConnection = true;
        }

        return;
      }
    }
  }

  bool StormSocketBackend::CheckDisconnectFlags(StormSocketConnectionId id, StormSocketDisconnectFlags::Index new_flags)
  {
    auto & connection = GetConnection(id);
    if ((new_flags & StormSocketDisconnectFlags::kAllFlags) == StormSocketDisconnectFlags::kAllFlags)
    {
#ifndef DISABLE_MBED
      FreeOutgoingPacket(connection.m_EncryptWriter);
#endif

      // Free the recv buffer
      connection.m_RecvBuffer.FreeBuffers();
      connection.m_DecryptBuffer.FreeBuffers();

      connection.m_SlotGen = (connection.m_SlotGen + 1) & 0xFF;
      FreeConnectionResources(id);

      if (m_HandshakeTimeout > 0)
      {
        std::lock_guard<std::mutex> lock(connection.m_TimeoutLock);
        m_Timeouts[id.GetIndex()]->cancel();
        m_Timeouts[id.GetIndex()] = std::experimental::nullopt;
      }

      FreeConnectionSlot(id);
      return true;
    }

    return false;
  }

  void StormSocketBackend::SetHandshakeComplete(StormSocketConnectionId id)
  {
    auto & connection = GetConnection(id);
    connection.m_HandshakeComplete.store(true);
  }

  StormSocketConnectionId StormSocketBackend::AllocateConnection(StormSocketFrontend * frontend, uint32_t remote_ip, uint16_t remote_port, bool for_connect, const void * init_data)
  {
    auto frontend_id = frontend->AllocateFrontendId();
    if (frontend_id == InvalidFrontendId)
    {
      return StormSocketConnectionId::InvalidConnectionId;
    }

    for (int index = 0; index < m_MaxConnections; index++)
    {
      auto & connection = GetConnection(index);
      if (connection.m_Used.test_and_set() == false)
      {
        // Set up the connection
        connection.m_DecryptBuffer = StormSocketBuffer(&m_Allocator, m_FixedBlockSize);
        connection.m_RecvBuffer = StormSocketBuffer(&m_Allocator, m_FixedBlockSize);
        connection.m_ParseBlock = InvalidBlockHandle;
        connection.m_UnparsedDataLength = 0;
        connection.m_ParseOffset = 0;
        connection.m_ReadOffset = 0;
        connection.m_RemoteIP = remote_ip;
        connection.m_RemotePort = remote_port;
        connection.m_PendingPackets = 0;
        connection.m_DisconnectFlags = 0;

        connection.m_SSLContext = SSLContext();
        connection.m_RecvCriticalSection = 0;

        connection.m_PendingSendBlockStart = InvalidBlockHandle;
        connection.m_PendingSendBlockCur = InvalidBlockHandle;
        connection.m_Transmitting = false;

        connection.m_PacketsRecved = 0;
        connection.m_PacketsSent = 0;
        connection.m_HandshakeComplete = false;
        connection.m_FailedConnection = false;
        connection.m_Closing = false;
        connection.m_Allocated = true;
        connection.m_SlotIndex = index;

        connection.m_RecvBuffer.InitBuffers();
#ifndef DISABLE_MBED
        connection.m_EncryptWriter = CreateWriter(true);
#endif

        auto connection_id = StormSocketConnectionId(index, connection.m_SlotGen);
        connection.m_Frontend = frontend;
        connection.m_FrontendId = frontend_id;

        if (m_HandshakeTimeout > 0)
        {
          auto handler = [=, slot_gen = connection.m_SlotGen](const asio::error_code& error)
          {
            if (!error)
            {
              std::lock_guard<std::mutex> lock(m_Connections[index].m_TimeoutLock);

              if (m_Connections[index].m_SlotGen == slot_gen && m_Connections[index].m_HandshakeComplete == false)
              {
                StormSocketLog("Handshake timeout\n");
                ForceDisconnect(connection_id);
              }
            }
          };

          std::lock_guard<std::mutex> lock(connection.m_TimeoutLock);
          m_Timeouts[index].emplace(m_IOService, std::chrono::steady_clock::now() + std::chrono::seconds(m_HandshakeTimeout));
          m_Timeouts[index]->async_wait(handler);
        }

        frontend->InitConnection(connection_id, frontend_id, init_data);

        if (for_connect == false)
        {
          connection.m_DisconnectFlags |= StormSocketDisconnectFlags::kConnectFinished;
          frontend->QueueConnectEvent(connection_id, connection.m_FrontendId, remote_ip, remote_port);
        }

        frontend->AssociateConnectionId(connection_id);
        return connection_id;
      }
    }

    frontend->FreeFrontendId(frontend_id);

    return StormSocketConnectionId::InvalidConnectionId;
  }

  void StormSocketBackend::FreeConnectionSlot(StormSocketConnectionId id)
  {
    auto & connection = GetConnection(id);
    connection.m_Allocated = false;
    connection.m_Used.clear();
  }

  void StormSocketBackend::PrepareToAccept(StormSocketBackendAcceptorId acceptor_id)
  {
    std::lock_guard<std::mutex> guard(m_AcceptorLock);
    auto acceptor_itr = m_Acceptors.find(acceptor_id);
    if (acceptor_itr == m_Acceptors.end())
    {
      return;
    }

    auto & acceptor = acceptor_itr->second;
    acceptor.m_AcceptSocket = asio::ip::tcp::socket(m_IOService);

    auto accept_callback = [this, acceptor_id](const asio::error_code & error)
    {
      AcceptNewConnection(error, acceptor_id);
      PrepareToAccept(acceptor_id);
    };

    acceptor.m_Acceptor.async_accept(acceptor.m_AcceptSocket, acceptor.m_AcceptEndpoint, accept_callback);
  }

  void StormSocketBackend::AcceptNewConnection(const asio::error_code & error, StormSocketBackendAcceptorId acceptor_id)
  {
    if (error)
    {
      printf("Accept error: %s (%d)\n", error.message().data(), error.value());
      return;
    }

    std::unique_lock<std::mutex> guard(m_AcceptorLock);

    auto acceptor_itr = m_Acceptors.find(acceptor_id);
    if (acceptor_itr == m_Acceptors.end())
    {
      printf("Acceptor not found\n");
      return;
    }

    auto & acceptor = acceptor_itr->second;
    auto & new_socket = acceptor.m_AcceptSocket;

    asio::error_code ec;
    new_socket.set_option(asio::ip::tcp::no_delay(true), ec);

    StormSocketConnectionId connection_id = AllocateConnection(acceptor.m_Frontend,
      acceptor.m_AcceptEndpoint.address().to_v4().to_ulong(), acceptor.m_AcceptEndpoint.port(), false, nullptr);

    if (connection_id == StormSocketConnectionId::InvalidConnectionId)
    {
      printf("Ran out of connection slots\n");
      new_socket.close();
      new_socket = asio::ip::tcp::socket(m_IOService);
      return;
    }

    m_ClientSockets[connection_id].emplace(std::move(new_socket));

    auto & connection = GetConnection(connection_id);

#ifndef DISABLE_MBED
    if (acceptor.m_Frontend->UseSSL(connection_id, connection.m_FrontendId))
    {
      auto ssl_config = acceptor.m_Frontend->GetSSLConfig(connection.m_FrontendId);

      mbedtls_ssl_init(&connection.m_SSLContext.m_SSLContext);
      mbedtls_ssl_setup(&connection.m_SSLContext.m_SSLContext, ssl_config);

      g_StormSocketsNumTlsConnections++;

      connection.m_DecryptBuffer.InitBuffers();

      auto send_callback = [](void * ctx, const unsigned char * data, size_t size) -> int
      {
        StormSocketConnectionBase * connection = (StormSocketConnectionBase *)ctx;
        connection->m_EncryptWriter.WriteByteBlock(data, 0, size);
        return (int)size;
      };

      auto recv_callback = [](void * ctx, unsigned char * data, size_t size) -> int
      {
        StormSocketConnectionBase * connection = (StormSocketConnectionBase *)ctx;
        auto read = connection->m_DecryptBuffer.BlockRead(data, (int)size);
        return read == 0 ? MBEDTLS_ERR_SSL_WANT_READ : read;
      };

      auto recv_timeout_callback = [](void * ctx, unsigned char * data, size_t size, uint32_t timeout) -> int
      {
        StormSocketConnectionBase * connection = (StormSocketConnectionBase *)ctx;
        auto read = connection->m_DecryptBuffer.BlockRead(data, (int)size);
        return read == 0 ? MBEDTLS_ERR_SSL_WANT_READ : read;
      };

      mbedtls_ssl_set_bio(&connection.m_SSLContext.m_SSLContext,
        &connection,
        send_callback,
        recv_callback,
        recv_timeout_callback);

      
      //mbedtls_debug_set_threshold(5);

    }
    else
#endif
    {
      connection.m_Frontend->ConnectionEstablishComplete(connection_id, connection.m_FrontendId);
    }

    guard.unlock();
    PrepareToRecv(connection_id);
  }

  void StormSocketBackend::PrepareToConnect(StormSocketConnectionId id, asio::ip::tcp::endpoint endpoint)
  {
    auto & connection = GetConnection(id);
    if ((connection.m_DisconnectFlags & StormSocketDisconnectFlags::kAllFlags) != 0)
    {
      SetDisconnectFlag(id, StormSocketDisconnectFlags::kConnectFinished);
      SetDisconnectFlag(id, StormSocketDisconnectFlags::kRecvThread);
      return;
    }

    connection.m_RemoteIP = endpoint.address().to_v4().to_ulong();

    auto connect_callback = [this, id](asio::error_code ec)
    {
      if (!ec)
      {
        FinalizeConnectToHost(id);
      }
      else
      {
        StormSocketLog("Failed to connect to server: %d\n", ec.value());
        ConnectFailed(id);
      }
    };

    m_ClientSockets[id]->async_connect(endpoint, connect_callback);
  }

  void StormSocketBackend::FinalizeConnectToHost(StormSocketConnectionId id)
  {
    auto & connection = GetConnection(id);

#ifndef DISABLE_MBED
    if (connection.m_Frontend->UseSSL(id, connection.m_FrontendId))
    {
      auto ssl_config = connection.m_Frontend->GetSSLConfig(connection.m_FrontendId);

      mbedtls_ssl_init(&connection.m_SSLContext.m_SSLContext);
      mbedtls_ssl_setup(&connection.m_SSLContext.m_SSLContext, ssl_config);

      g_StormSocketsNumTlsConnections++;

      connection.m_DecryptBuffer.InitBuffers();

      auto send_callback = [](void * ctx, const unsigned char * data, size_t size) -> int
      {
        StormSocketConnectionBase * connection = (StormSocketConnectionBase *)ctx;
        connection->m_EncryptWriter.WriteByteBlock(data, 0, size);
        return (int)size;
      };

      auto recv_callback = [](void * ctx, unsigned char * data, size_t size) -> int
      {
        StormSocketConnectionBase * connection = (StormSocketConnectionBase *)ctx;
        auto read = connection->m_DecryptBuffer.BlockRead(data, (int)size);
        return read == 0 ? MBEDTLS_ERR_SSL_WANT_READ : read;
      };

      auto recv_timeout_callback = [](void * ctx, unsigned char * data, size_t size, uint32_t timeout) -> int
      {
        StormSocketConnectionBase * connection = (StormSocketConnectionBase *)ctx;
        auto read = connection->m_DecryptBuffer.BlockRead(data, (int)size);
        return read == 0 ? MBEDTLS_ERR_SSL_WANT_READ : read;
      };

      mbedtls_ssl_set_bio(&connection.m_SSLContext.m_SSLContext,
        &connection,
        send_callback,
        recv_callback,
        recv_timeout_callback);

      int ec = mbedtls_ssl_handshake(&connection.m_SSLContext.m_SSLContext);

      char error_str[1024];
      mbedtls_strerror(ec, error_str, sizeof(error_str));

      if (connection.m_EncryptWriter.m_PacketInfo->m_TotalLength > 0)
      {
        SendPacketToConnection(connection.m_EncryptWriter, id);
        FreeOutgoingPacket(connection.m_EncryptWriter);
        connection.m_EncryptWriter = CreateWriter(true);
      }
    }
    else
#endif
    {
      connection.m_Frontend->ConnectionEstablishComplete(id, connection.m_FrontendId);
    }

    connection.m_Frontend->QueueConnectEvent(id, connection.m_FrontendId, connection.m_RemoteIP, connection.m_RemotePort);

    PrepareToRecv(id);
    SetDisconnectFlag(id, StormSocketDisconnectFlags::kConnectFinished);
  }

  void StormSocketBackend::ConnectFailed(StormSocketConnectionId id)
  {
    SetSocketDisconnected(id);
    SetDisconnectFlag(id, StormSocketDisconnectFlags::kConnectFinished);
    SetDisconnectFlag(id, StormSocketDisconnectFlags::kRecvThread);
  }

  void StormSocketBackend::ProcessNewData(StormSocketConnectionId connection_id, const asio::error_code & error, std::size_t bytes_received)
  {
    auto & connection = GetConnection(connection_id);

    if (!error)
    {
#ifndef DISABLE_MBED
      if (connection.m_Frontend->UseSSL(connection_id, connection.m_FrontendId))
      {
        connection.m_DecryptBuffer.GotData((int)bytes_received);
        while (connection.m_SSLContext.m_SSLHandshakeComplete == false)
        {
          int ec = mbedtls_ssl_handshake(&connection.m_SSLContext.m_SSLContext);

          char error_str[1024];
          mbedtls_strerror(ec, error_str, sizeof(error_str));

          if (connection.m_EncryptWriter.m_PacketInfo->m_TotalLength > 0)
          {
            SendPacketToConnection(connection.m_EncryptWriter, connection_id);
            FreeOutgoingPacket(connection.m_EncryptWriter);
            connection.m_EncryptWriter = CreateWriter(true);
          }

          if (ec == 0)
          {
            connection.m_SSLContext.m_SSLHandshakeComplete = true;
            connection.m_Frontend->ConnectionEstablishComplete(connection_id, connection.m_FrontendId);
          }
          else if (ec != MBEDTLS_ERR_SSL_WANT_READ)
          {
            if(ec != MBEDTLS_ERR_SSL_CONN_EOF)
            {
              StormSocketLog("SSL error: %d\n", ec);
            }

            SetSocketDisconnected(connection_id);
            SetDisconnectFlag(connection_id, StormSocketDisconnectFlags::kRecvThread);
            return;
          }
          else
          {
            PrepareToRecv(connection_id);
            return;
          }
        }
      }
      else
#endif
      {
        // Data just goes directly into the recv buffer
        connection.m_RecvBuffer.GotData((int)bytes_received);
        connection.m_UnparsedDataLength.fetch_add((int)bytes_received);
      }

      uint64_t prof = Profiling::StartProfiler();
      TryProcessReceivedData(connection_id, false);
      Profiling::EndProfiler(prof, ProfilerCategory::kProcData);
    }
    else
    {
      uint64_t prof = Profiling::StartProfiler();
      TryProcessReceivedData(connection_id, true);
      Profiling::EndProfiler(prof, ProfilerCategory::kProcData);
    }
  }


  void StormSocketBackend::TryProcessReceivedData(StormSocketConnectionId connection_id, bool recv_failure)
  {
    if (ProcessReceivedData(connection_id, recv_failure) == false)
    {
      auto recheck_callback = [=]()
      {
        TryProcessReceivedData(connection_id, recv_failure);
      };

      ProfileScope prof(ProfilerCategory::kRepost);
      m_IOService.post(recheck_callback);
    }
    else
    {
      if (recv_failure == false)
      {
        PrepareToRecv(connection_id);
      }
    }
  }

  void StormSocketBackend::SignalOutgoingSocket(StormSocketConnectionId id, StormSocketIOOperationType::Index type, std::size_t size)
  {
    int send_thread_index = id % m_NumSendThreads;

    StormSocketIOOperation op;
    op.m_ConnectionId = id;
    op.m_Type = type;
    op.m_Size = (int)size;

    while (m_SendQueue[send_thread_index].Enqueue(op, 0, m_SendQueueIncdices.get(), m_SendQueueArray.get()) == false)
    {
      std::this_thread::yield();
    }

    m_SendThreadSemaphores[send_thread_index].Release();
  }

  bool StormSocketBackend::ProcessReceivedData(StormSocketConnectionId connection_id, bool recv_failure)
  {
    auto & connection = GetConnection(connection_id);

    // Lock the receiver
    int old_val = 0;
    if (std::atomic_compare_exchange_weak(&connection.m_RecvCriticalSection, &old_val, 1) == false)
    {
      return false;
    }

    if (connection.m_SlotGen != connection_id.GetGen())
    {
      return true;
    }

#ifndef DISABLE_MBED
    if (connection.m_Frontend->UseSSL(connection_id, connection.m_FrontendId))
    {
      auto prof = ProfileScope(ProfilerCategory::kSSLDecrypt);
      while (true)
      {
        StormSocketBufferWriteInfo pointer_info;
        if (connection.m_RecvBuffer.GetPointerInfo(pointer_info) == false)
        {
          throw std::runtime_error("Error getting pointer info for recv buffer");
        }

        int ret = mbedtls_ssl_read(&connection.m_SSLContext.m_SSLContext, (uint8_t *)pointer_info.m_Ptr1, pointer_info.m_Ptr1Size);
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
        {
          connection.m_RecvBuffer.GotData(0);
          break;
        }

        if (ret < 0)
        {
          connection.m_RecvBuffer.GotData(0);
          break;
        }
        else
        {
          connection.m_UnparsedDataLength.fetch_add(ret);
          connection.m_RecvBuffer.GotData(ret);
        }
      }
    }
#endif

    bool success = connection.m_Frontend->ProcessData(connection_id, connection.m_FrontendId);
    if (recv_failure)
    {
      SetSocketDisconnected(connection_id);
      connection.m_RecvCriticalSection.store(0);
      SetDisconnectFlag(connection_id, StormSocketDisconnectFlags::kRecvThread);
      success = true;
    }
    else
    {
      connection.m_RecvCriticalSection.store(0);
    }

    return success;
  }

  void StormSocketBackend::PrepareToRecv(StormSocketConnectionId connection_id)
  {
    auto & connection = GetConnection(connection_id);

#ifndef DISABLE_MBED
    StormSocketBuffer * buffer = connection.m_Frontend->UseSSL(connection_id, connection.m_FrontendId) ? &connection.m_DecryptBuffer : &connection.m_RecvBuffer;
#else
    StormSocketBuffer * buffer = &connection.m_RecvBuffer;
#endif

    StormSocketBufferWriteInfo pointer_info;
    if (buffer->GetPointerInfo(pointer_info) == false)
    {
      throw std::runtime_error("Error getting pointer info for recv buffer");
    }

    std::array<asio::mutable_buffer, 2> buffer_set =
    {
      asio::buffer(pointer_info.m_Ptr1, pointer_info.m_Ptr1Size),
      asio::buffer(pointer_info.m_Ptr2, pointer_info.m_Ptr2Size)
    };

    auto recv_callback = [=](const asio::error_code & error, size_t bytes_received) { ProcessNewData(connection_id, error, bytes_received); };
    m_ClientSockets[connection_id]->async_read_some(buffer_set, recv_callback);
  }

  void StormSocketBackend::IOThreadMain()
  {
    while (m_ThreadStopRequested == false)
    {
      if (m_IOService.run() == 0)
      {
        if (m_IOService.stopped())
        {
          auto val = m_IOResetCount.fetch_add(1);
          if (val == m_NumIOThreads - 1)
          {
            m_IOService.reset();
            m_IOResetCount = 0;
            m_IOResetSemaphore.Release(m_NumIOThreads - 1);
          }
          else
          {
            while (m_IOResetSemaphore.WaitOne(1) == false)
            {
              if (m_ThreadStopRequested)
              {
                return;
              }
            }
          }
        }
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }


  void StormSocketBackend::SendThreadMain(int thread_index)
  {
    StormSocketIOOperation op;

    StormMessageWriter writer;

    while (m_ThreadStopRequested == false)
    {
      m_SendThreadSemaphores[thread_index].WaitOne(100);

      while (m_SendQueue[thread_index].TryDequeue(op, 0, m_SendQueueIncdices.get(), m_SendQueueArray.get()))
      {
        StormSocketConnectionId connection_id = op.m_ConnectionId;
        int connection_gen = connection_id.GetGen();
        auto & connection = GetConnection(connection_id);

        if (op.m_Type == StormSocketIOOperationType::FreePacket)
        {
          if (connection_gen != connection.m_SlotGen)
          {
            continue;
          }

          connection.m_Transmitting = false;

          bool bail = false;

          StormFixedBlockHandle block_handle = connection.m_PendingSendBlockStart;
          while (op.m_Size > 0)
          {
            if (block_handle == InvalidBlockHandle)
            {
              bail = true;
              break;
            }

            StormPendingSendBlock * send_block = (StormPendingSendBlock *)m_PendingSendBlocks.ResolveHandle(block_handle);

            if (op.m_Size >= send_block->m_DataLen)
            {
              op.m_Size -= send_block->m_DataLen;
              block_handle = ReleasePendingSendBlock(block_handle, send_block);
            }
            else
            {
              send_block->m_DataLen -= op.m_Size;
              send_block->m_DataStart = Marshal::MemOffset(send_block->m_DataStart, op.m_Size);
              break;
            }
          }

          if (bail == false)
          {
            connection.m_PendingSendBlockStart = block_handle;
            if (block_handle == InvalidBlockHandle)
            {
              connection.m_PendingSendBlockCur = block_handle;

              if (connection.m_Closing)
              {
                asio::error_code ec;
                m_ClientSockets[connection_id]->shutdown(asio::socket_base::shutdown_send, ec);
                SignalCloseThread(connection_id);
              }
            }

            TransmitConnectionPackets(connection_id);
          }
        }
        else if (op.m_Type == StormSocketIOOperationType::ClearQueue)
        {
          if (connection_gen != connection.m_SlotGen)
          {
            continue;
          }

          ReleaseSendQueue(connection_id, connection_gen);
          SetDisconnectFlag(connection_id, StormSocketDisconnectFlags::kSendThread);
          SignalCloseThread(connection_id);
        }
        else if (op.m_Type == StormSocketIOOperationType::Close)
        {
          if (connection_gen != connection.m_SlotGen)
          {
            continue;
          }

          connection.m_Closing = true;
          if (connection.m_PendingSendBlockStart == InvalidBlockHandle)
          {
            asio::error_code ec;
            m_ClientSockets[connection_id]->shutdown(asio::socket_base::shutdown_send, ec);
            SignalCloseThread(connection_id);
          }
        }
        else if (op.m_Type == StormSocketIOOperationType::QueuePacket)
        {
          if (connection_gen != connection.m_SlotGen)
          {
            continue;
          }

          if ((connection.m_DisconnectFlags & StormSocketDisconnectFlags::kSendThread) != 0)
          {
            continue;
          }

          if (connection.m_Closing)
          {
            continue;
          }

          if (m_OutputQueue[connection_id].TryDequeue(writer, connection_gen, m_OutputQueueIncdices.get(), m_OutputQueueArray.get()))
          {
            uint64_t prof = Profiling::StartProfiler();

#ifndef DISABLE_MBED
            if (writer.m_IsEncrypted == false && connection.m_Frontend->UseSSL(connection_id, connection.m_FrontendId))
            {
              StormMessageWriter encrypted = EncryptWriter(connection_id, writer);
              FreeOutgoingPacket(writer);

              writer = encrypted;
            }
#endif

            StormFixedBlockHandle block_handle = writer.m_PacketInfo->m_StartBlock;;
            int header_offset = writer.m_PacketInfo->m_SendOffset;
            int pending_data = writer.m_PacketInfo->m_TotalLength;

            while (block_handle != InvalidBlockHandle)
            {
              int potential_data_in_block = m_FixedBlockSize - header_offset - (writer.m_ReservedHeaderLength + writer.m_ReservedTrailerLength);
              int block_len = std::min(pending_data, potential_data_in_block);
              int data_start = writer.m_ReservedHeaderLength - writer.m_HeaderLength + header_offset;
              int data_length = writer.m_HeaderLength + block_len + writer.m_TrailerLength;

              void * block = m_Allocator.ResolveHandle(block_handle);
              block_handle = m_Allocator.GetNextBlock(block_handle);

              StormFixedBlockHandle outgoing_block_handle = m_PendingSendBlocks.AllocateBlock(StormFixedBlockType::SendBlock);
              StormPendingSendBlock * outgoing_block = (StormPendingSendBlock *)m_PendingSendBlocks.ResolveHandle(outgoing_block_handle);

              outgoing_block->m_DataLen = data_length;
              outgoing_block->m_DataStart = Marshal::MemOffset(block, data_start);

              if (block_handle == InvalidBlockHandle)
              {
                outgoing_block->m_RefCount = &writer.m_PacketInfo->m_RefCount;
                outgoing_block->m_StartBlock = writer.m_PacketInfo->m_StartBlock;
                outgoing_block->m_PacketHandle = writer.m_PacketHandle;
              }
              else
              {
                outgoing_block->m_RefCount = nullptr;
              }

              if (connection.m_PendingSendBlockCur != InvalidBlockHandle)
              {
                m_PendingSendBlocks.SetNextBlock(connection.m_PendingSendBlockCur, outgoing_block_handle);
              }
              else
              {
                connection.m_PendingSendBlockStart = outgoing_block_handle;
              }

              connection.m_PendingSendBlockCur = outgoing_block_handle;

              header_offset = 0;
              pending_data -= block_len;
            }

            TransmitConnectionPackets(connection_id);

            Profiling::EndProfiler(prof, ProfilerCategory::kSend);
          }
        }
      }
    }
  }

  void StormSocketBackend::TransmitConnectionPackets(StormSocketConnectionId connection_id)
  {
    auto & connection = GetConnection(connection_id);
    if (connection.m_Transmitting)
    {
      return;
    }

    if ((connection.m_DisconnectFlags & (int)StormSocketDisconnectFlags::kSignalClose) != 0)
    {
      return;
    }

    StormFixedBlockHandle block_handle = connection.m_PendingSendBlockStart;
    if (block_handle == InvalidBlockHandle)
    {
      return;
    }

    SendBuffer buffer_set;
    int buffer_size = 0;
    int total_size = 0;

    for (buffer_size = 0; buffer_size < kBufferSetCount; buffer_size++)
    {
      if (block_handle == InvalidBlockHandle)
      {
        break;
      }

      StormPendingSendBlock * send_block = (StormPendingSendBlock *)m_PendingSendBlocks.ResolveHandle(block_handle);
      buffer_set[buffer_size] = asio::buffer(send_block->m_DataStart, send_block->m_DataLen);

      total_size += send_block->m_DataLen;

      block_handle = m_PendingSendBlocks.GetNextBlock(block_handle);
    }

    if (buffer_size > 0)
    {
      auto send_callback = [=](const asio::error_code & error, std::size_t bytes_transfered)
      {
        if (!error)
        {
          SignalOutgoingSocket(connection_id, StormSocketIOOperationType::FreePacket, bytes_transfered);
        }
        else
        {
          SetSocketDisconnected(connection_id);
        }
      };

      connection.m_Transmitting = true;
      m_ClientSockets[connection_id]->async_send(buffer_set, send_callback);
    }
  }


  StormFixedBlockHandle StormSocketBackend::ReleasePendingSendBlock(StormFixedBlockHandle send_block_handle, StormPendingSendBlock * send_block)
  {
    if (send_block->m_RefCount)
    {
      if (send_block->m_RefCount->fetch_sub(1) == 1)
      {
        m_Allocator.FreeBlockChain(send_block->m_StartBlock, StormFixedBlockType::BlockMem);
        m_MessageSenders.FreeBlock(send_block->m_PacketHandle, StormFixedBlockType::Sender);
      }
    }

    return m_PendingSendBlocks.FreeBlock(send_block_handle, StormFixedBlockType::SendBlock);
  }

  void StormSocketBackend::ReleaseSendQueue(StormSocketConnectionId connection_id, int connection_gen)
  {
    StormMessageWriter writer;
    // Lock the queue so that nothing else can put packets into it
    m_OutputQueue[connection_id].Lock(connection_gen + 1, m_OutputQueueIncdices.get(), m_OutputQueueArray.get());

    // Drain the remaining packets
    while (m_OutputQueue[connection_id].TryDequeue(writer, connection_gen + 1, m_OutputQueueIncdices.get(), m_OutputQueueArray.get()))
    {
      if (writer.m_PacketInfo != NULL)
      {
        FreeOutgoingPacket(writer);
      }
    }

    m_OutputQueue[connection_id].Reset(connection_gen + 1, m_OutputQueueIncdices.get(), m_OutputQueueArray.get());
  }

  StormMessageWriter StormSocketBackend::EncryptWriter(StormSocketConnectionId connection_id, StormMessageWriter & writer)
  {
    auto prof = ProfileScope(ProfilerCategory::kSSLEncrypt);
#ifndef DISABLE_MBED
    auto & connection = GetConnection(connection_id);
    StormFixedBlockHandle cur_block = writer.m_PacketInfo->m_StartBlock;

    int data_to_encrypt = writer.m_PacketInfo->m_TotalLength;

    int block_index = 0;
    while (cur_block != InvalidBlockHandle)
    {
      void * block_base = m_Allocator.ResolveHandle(cur_block);
      int start_offset = (block_index == 0 ? writer.m_PacketInfo->m_SendOffset : 0);
      int header_offset = writer.m_ReservedHeaderLength + start_offset;
      int block_size = writer.m_Allocator->GetBlockSize() - (writer.m_ReservedHeaderLength + writer.m_ReservedTrailerLength + start_offset);

      int data_size = std::min(data_to_encrypt, block_size);

      void * block_mem = Marshal::MemOffset(block_base, header_offset);
      int ec = mbedtls_ssl_write(&connection.m_SSLContext.m_SSLContext, (uint8_t *)block_mem, data_size);
      if (ec < 0)
      {
        throw std::runtime_error("Error encrypting packet");
      }

      data_to_encrypt -= data_size;

      cur_block = m_Allocator.GetNextBlock(cur_block);
      block_index++;
    }

    StormMessageWriter encrypted = connection.m_EncryptWriter;
    connection.m_EncryptWriter = CreateWriter(true);

    return encrypted;
#else

    return writer;

#endif
  }

  void StormSocketBackend::CloseSocketThread()
  {
    StormSocketConnectionId id;
    while (m_ThreadStopRequested == false)
    {
      m_CloseConnectionSemaphore.WaitOne();

      while (m_ClosingConnectionQueue.TryDequeue(id))
      {
        if (m_Connections[id].m_PendingSendBlockStart != InvalidBlockHandle)
        {
          //printf("bad\n");
        }

        CloseSocket(id);
        SetSocketDisconnected(id);
        SetDisconnectFlag(id, StormSocketDisconnectFlags::kThreadClose);
      }
    }
  }

  void StormSocketBackend::QueueCloseSocket(StormSocketConnectionId id)
  {
    if (m_ClosingConnectionQueue.Enqueue(id))
    {
      m_CloseConnectionSemaphore.Release();
    }
    else
    {
      CloseSocket(id);
      SetSocketDisconnected(id);
      SetDisconnectFlag(id, StormSocketDisconnectFlags::kThreadClose);
    }
  }

  void StormSocketBackend::CloseSocket(StormSocketConnectionId id)
  {
    asio::error_code ec;
    m_ClientSockets[id]->shutdown(asio::socket_base::shutdown_receive, ec);
  }

  void StormSocketBackend::FreeConnectionResources(StormSocketConnectionId id)
  {
    auto & connection = GetConnection(id);
#ifndef DISABLE_MBED

    if (connection.m_Frontend->UseSSL(id, connection.m_FrontendId))
    {
      mbedtls_ssl_free(&connection.m_SSLContext.m_SSLContext);
      g_StormSocketsNumTlsConnections--;
    }
#endif

    StormFixedBlockHandle block_handle = connection.m_PendingSendBlockStart;
    while (block_handle != InvalidBlockHandle)
    {
      block_handle = ReleasePendingSendBlock(block_handle, (StormPendingSendBlock *)m_PendingSendBlocks.ResolveHandle(block_handle));
    }

    connection.m_Frontend->CleanupConnection(id, connection.m_FrontendId);
    connection.m_Frontend->DisassociateConnectionId(id);
    connection.m_Frontend->FreeFrontendId(connection.m_FrontendId);

    asio::error_code ec;

    m_ClientSockets[id]->close(ec);
    m_ClientSockets[id] = std::experimental::nullopt;
  }
}
