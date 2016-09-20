//
//#include "StormSocketServerWin.h"
//
//#ifdef USE_WINSEC
//#include <sspi.h>
//#include <schnlsp.h>
//#include <ntsecapi.h>
//
//#pragma comment(lib, "Crypt32.lib")
//#pragma comment(lib, "Secur32.lib")
//#endif
//
//#include <Ws2tcpip.h>
//#include <fstream>
//#include <stdexcept>
//
//
//#ifdef _WINDOWS
//#pragma comment(lib, "Ws2_32.lib")
//#pragma comment(lib, "mswsock.lib")
//
//
//
//namespace StormSockets
//{
//	StormSocketServerWin::StormSocketServerWin(StormSocketInitSettings & settings)
//		: StormSocketServerFrontendBase(settings)
//	{
//    m_IOThreads = nullptr;
//    m_SendThreads = nullptr;
//
//		// Alocate memory for the send/recv overlaps
//		// This lets us identify which connection had an event with GetQueuedCompletionStatus
//		m_SendOverlaps = std::make_unique<OVERLAPPED[]>(settings.MaxConnections);
//		m_RecvOverlaps = std::make_unique<OVERLAPPED[]>(settings.MaxConnections);
//
//		memset(&m_AcceptOverlap, 0, sizeof(OVERLAPPED));
//		memset(&m_RecheckOverlap, 0, sizeof(OVERLAPPED));
//    memset(m_SendOverlaps.get(), 0, sizeof(OVERLAPPED) * settings.MaxConnections);
//    memset(m_RecvOverlaps.get(), 0, sizeof(OVERLAPPED) * settings.MaxConnections);
//
//		m_EndSendOverlaps = m_SendOverlaps.get() + settings.MaxConnections;
//
//		memset(m_AcceptBuffer, 0, sizeof(m_AcceptBuffer));
//
//		m_IoCompletionPort = NULL;
//		m_AcceptSocket = InvalidSocketId;
//		m_ListenSocket = InvalidSocketId;
//
//    inet_pton(AF_INET, settings.LocalInterface, &m_LocalInterface);
//		m_Port = settings.Port;
//    m_UseSSL = false;
//
//#ifdef USE_WINSEC
//    m_HeaderLength = 0;
//    m_TrailerLength = 0;
//#endif
//
//    if (settings.CertificateFile && settings.PrivateKeyFile != nullptr)
//    {
//      std::ifstream cert_file;
//      cert_file.open(settings.CertificateFile, std::ifstream::in | std::ifstream::binary);
//      cert_file.seekg(0, std::ios::end);
//      int cert_file_length = (int)cert_file.tellg();
//      cert_file.seekg(0, std::ios::beg);
//
//      std::unique_ptr<char[]> cert_data = std::make_unique<char[]>(cert_file_length);
//      cert_file.read(cert_data.get(), cert_file_length);
//      cert_file.close();
//
//#ifdef USE_MBED
//
//#endif
//
//#ifdef USE_WINSEC
//      CRYPT_DATA_BLOB blob;
//      blob.pbData = (BYTE *)cert_data.get();
//      blob.cbData = cert_file_length;
//
//      HCERTSTORE cert_store = PFXImportCertStore(&blob, settings.PrivateKeyFile, 0);
//
//      if (cert_store == NULL)
//      {
//        throw std::runtime_error("Failed to load pkcs12 certificate file");
//      }
//
//      m_CertContext = CertFindCertificateInStore(cert_store,
//        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_ANY, NULL, NULL);
//
//      if (m_CertContext == NULL)
//      {
//        throw std::runtime_error("Failed to load pkcs12 certificate file");
//      }
//
//      SCHANNEL_CRED cred;
//      memset(&cred, 0, sizeof(SCHANNEL_CRED));
//      cred.dwVersion = SCHANNEL_CRED_VERSION;
//      cred.grbitEnabledProtocols = SP_PROT_TLS1_2_SERVER;
//      cred.dwFlags = SCH_CRED_NO_SYSTEM_MAPPER;
//      cred.cCreds = 1;
//      cred.paCred = &m_CertContext;
//
//      int result = AcquireCredentialsHandle(
//        NULL,
//        UNISP_NAME,
//        SECPKG_CRED_INBOUND,
//        NULL,
//        &cred,
//        NULL,
//        NULL,
//        &m_ServerSSLContext,
//        NULL);
//
//      if (result != 0)
//      {
//        throw std::runtime_error("Failed to acquire security credentials");
//      }
//
//      m_HeaderLength = 13;
//      m_TrailerLength = 16;
//      m_UseSSL = true;
//#endif
//    }
//	}
//
//	StormSocketServerWin::~StormSocketServerWin()
//	{
//    CloseSocketInternal(m_ListenSocket);
//    CloseSocketInternal(m_AcceptSocket);
//		CloseHandle(m_IoCompletionPort);
//
//    if (m_IOThreads)
//    {
//      for (int index = 0; index < m_NumIOThreads; index++)
//      {
//        m_IOThreads[index].join();
//      }
//    }
//
//    if (m_SendThreads)
//    {
//      for (int index = 0; index < m_NumSendThreads; index++)
//      {
//        m_SendThreadSemaphores[index].Release();
//        m_SendThreads[index].join();
//      }
//    }
//
//    if (m_UseSSL)
//    {
//#ifdef USE_WINSEC
//      FreeCredentialsHandle(&m_ServerSSLContext);
//#endif
//    }
//	}
//
//	void StormSocketServerWin::Start()
//	{
//		// Create the initial listening socket
//		WSADATA dummy;
//		int error = WSAStartup((short)0x0202, &dummy);
//		if (error != 0)
//		{
//			throw std::runtime_error("WSAStartup failed " + std::to_string(error));
//		}
//
//		m_ListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
//		if (m_ListenSocket == InvalidSocketId)
//		{
//			throw std::runtime_error("WSA Socket returned error code " + std::to_string(WSAGetLastError()));
//		}
//
//		// Use the disconnect buffer to extract the DisconnectEx function
//		GUID disconnectex_guid = WSAID_DISCONNECTEX;
//		DWORD garbage = 0;
//		error = WSAIoctl(m_ListenSocket,
//			SIO_GET_EXTENSION_FUNCTION_POINTER,
//			&disconnectex_guid,
//			sizeof(disconnectex_guid),
//			&DisconnectEx,
//			sizeof(DisconnectEx),
//			&garbage, NULL, NULL);
//
//		// Now the buffer can be used as an overlapped pointer
//		memset(&m_DisconnectBuffer, 0, sizeof(OVERLAPPED));
//
//		// Bind, listen, etc
//		sockaddr_in sin;
//		sin.sin_family = AF_INET;
//		sin.sin_addr.S_un.S_addr = m_LocalInterface;
//		sin.sin_port = (uint16_t)Marshal::HostToNetworkOrder((uint16_t)m_Port);
//
//		int addr_size = sizeof(sin);
//		error = bind(m_ListenSocket, (sockaddr *)&sin, addr_size);
//		if (error != 0)
//		{
//			throw std::runtime_error("Could not bind socket " + std::to_string(WSAGetLastError()));
//		}
//
//		error = listen(m_ListenSocket, 64);
//		if (error != 0)
//		{
//			throw std::runtime_error("Error creating listening socket " + std::to_string(error));
//		}
//
//		m_IoCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
//		if (m_IoCompletionPort == NULL)
//		{
//			throw std::runtime_error("Creating IO completion port failed " + std::to_string(GetLastError()));
//		}
//
//		if (CreateIoCompletionPort((HANDLE)m_ListenSocket, m_IoCompletionPort, (ULONG_PTR)-1, 0) == NULL)
//		{
//			throw std::runtime_error("Could not add listen socket to IO completion port " + std::to_string(GetLastError()));
//		}
//
//		// Start the io threads
//		m_IOThreads = std::make_unique<std::thread[]>(m_NumIOThreads);
//		for (int index = 0; index < m_NumIOThreads; index++)
//		{
//			m_IOThreads[index] = std::thread(&StormSocketServerWin::IOThreadMain, this);
//		}
//
//		m_SendThreads = std::make_unique<std::thread[]>(m_NumSendThreads);
//		for (int index = 0; index < m_NumSendThreads; index++)
//		{
//			m_SendThreads[index] = std::thread(&StormSocketServerWin::SendThreadMain, this, index);
//		}
//
//		PrepareToAccept();
//	}
//
//	void StormSocketServerWin::PrepareToAccept()
//	{
//		m_AcceptSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
//		if (m_AcceptSocket == InvalidSocketId)
//		{
//			throw std::runtime_error("WSA Socket returned error code " + std::to_string(WSAGetLastError()));
//		}
//
//		DWORD recv_length;
//		int sin_length = sizeof(sockaddr_in);
//
//		BOOL success = AcceptEx(
//			m_ListenSocket, m_AcceptSocket, m_AcceptBuffer, 0, sin_length + 16, sin_length + 16, &recv_length, &m_AcceptOverlap);
//
//		if (success)
//		{
//			AcceptNewConnection();
//		}
//	}
//
//	void StormSocketServerWin::AcceptNewConnection()
//	{
//		sockaddr_in sin;
//
//		// Why is the address 38 bytes from the start?  who knows!
//		memcpy(&sin, &m_AcceptBuffer[38], sizeof(sin));
//
//		StormSocketConnectionId connection_id = AllocateConnection(m_AcceptSocket, sin.sin_addr.S_un.S_addr, sin.sin_port);
//
//		if (connection_id == StormSocketConnectionId::InvalidConnectionId)
//		{
//      CloseSocketInternal(m_AcceptSocket);
//
//			PrepareToAccept();
//			return;
//		}
//
//		int error;
//
//		// Disable nagle
//		BOOL nodelay_val = 1;
//		error = setsockopt(m_AcceptSocket, SOL_SOCKET, TCP_NODELAY, (char *)&nodelay_val, sizeof(nodelay_val));
//
//		// Turn on linger so that our close packets make it through
//		LINGER linger;
//		linger.l_onoff = 1;
//		linger.l_linger = 1;
//		error = setsockopt(m_AcceptSocket, SOL_SOCKET, SO_LINGER, (char *)&linger, sizeof(linger));
//
//		u_long non_blocking = 1;
//		error = ioctlsocket(m_AcceptSocket, FIONBIO, &non_blocking);
//
//		if (CreateIoCompletionPort((HANDLE)m_AcceptSocket, m_IoCompletionPort, ULONG_PTR(connection_id.m_Index.Raw), 0) == NULL)
//		{
//			throw std::runtime_error("Could not add listen socket to IO completion port " + std::to_string(GetLastError()));
//		}
//
//    m_Connections[connection_id].m_RecvBuffer.InitBuffers();
//    if (m_UseSSL)
//    {
//      m_Connections[connection_id].m_DecryptBuffer.InitBuffers();
//    }
//
//		PrepareToRecv(connection_id);
//		PrepareToAccept();
//	}
//
//	void StormSocketServerWin::ProcessNewData(StormSocketConnectionId connection_id, int bytes_received)
//	{
//    printf("Got %d bytes\n", bytes_received);
//
//    if (m_UseSSL)
//    {
//      m_Connections[connection_id].m_DecryptBuffer.GotData(bytes_received);
//      if (m_Connections[connection_id].m_SSLContext.m_SSLHandshakeComplete == false)
//      {
//#ifdef USE_WINSEC
//        StormFixedBlockHandle inp_block = m_Connections[connection_id].m_DecryptBuffer.m_BlockStart;
//        int block_size = m_FixedBlockSize - m_Connections[connection_id].m_DecryptBuffer.m_ReadOffset;
//
//        int buffer_index = 0;
//        int data_avail = m_Connections[connection_id].m_DecryptBuffer.m_DataAvail;
//        int block_offset = m_Connections[connection_id].m_DecryptBuffer.m_ReadOffset;
//
//        SecBuffer inp_buffer[8];
//        SecBuffer outp_buffer[8];
//
//        for (buffer_index = 0; inp_block != InvalidBlockHandle && data_avail != 0; buffer_index++)
//        {
//          int data_in_buffer = block_size > data_avail ? data_avail : block_size;
//
//          inp_buffer[buffer_index].BufferType = SECBUFFER_TOKEN;
//          inp_buffer[buffer_index].cbBuffer = data_in_buffer;
//          inp_buffer[buffer_index].pvBuffer = (char *)m_Allocator.ResolveHandle(inp_block) + block_offset;
//
//          inp_block = m_Allocator.GetNextBlock(inp_block);
//          block_offset = 0;
//          block_size = m_FixedBlockSize;
//          data_avail -= data_in_buffer;
//        }
//
//        inp_buffer[buffer_index].BufferType = SECBUFFER_EMPTY;
//        inp_buffer[buffer_index].cbBuffer = 0;
//        inp_buffer[buffer_index].pvBuffer = NULL;
//        buffer_index++;
//
//        SecBufferDesc input_buffer;
//        input_buffer.cBuffers = buffer_index;
//        input_buffer.ulVersion = SECBUFFER_VERSION;
//        input_buffer.pBuffers = inp_buffer;
//
//        char alert_buffer[1024];
//
//        StormFixedBlockHandle outp_block = m_Allocator.AllocateBlock(StormFixedBlockType::BlockMem);
//        outp_buffer[0].BufferType = SECBUFFER_TOKEN;
//        outp_buffer[0].cbBuffer = m_FixedBlockSize;
//        outp_buffer[0].pvBuffer = m_Allocator.ResolveHandle(outp_block);
//        outp_buffer[1].BufferType = SECBUFFER_EMPTY;
//        outp_buffer[1].cbBuffer = 0;
//        outp_buffer[1].pvBuffer = NULL;
//        outp_buffer[2].BufferType = SECBUFFER_ALERT;
//        outp_buffer[2].cbBuffer = sizeof(alert_buffer);
//        outp_buffer[2].pvBuffer = alert_buffer;
//
//        SecBufferDesc output_buffer;
//        output_buffer.cBuffers = 3;
//        output_buffer.ulVersion = SECBUFFER_VERSION;
//        output_buffer.pBuffers = outp_buffer;
//
//
//        int return_val;
//        if (m_Connections[connection_id].m_SSLContext.m_SecHandleInitialized)
//        {
//          return_val = AcceptSecurityContext(
//            &m_ServerSSLContext,
//            &m_Connections[connection_id].m_SSLContext.m_SecHandle,
//            &input_buffer,
//            m_Connections[connection_id].m_SSLContext.m_SecAttribs,
//            0,
//            &m_Connections[connection_id].m_SSLContext.m_SecHandle,
//            &output_buffer,
//            &m_Connections[connection_id].m_SSLContext.m_SecAttribs,
//            &m_Connections[connection_id].m_SSLContext.m_SecTimestamp
//            );
//        }
//        else
//        {
//          m_Connections[connection_id].m_SSLContext.m_SecAttribs = ASC_REQ_STREAM;
//          return_val = AcceptSecurityContext(
//            &m_ServerSSLContext,
//            NULL,
//            &input_buffer,
//            m_Connections[connection_id].m_SSLContext.m_SecAttribs,
//            0,
//            &m_Connections[connection_id].m_SSLContext.m_SecHandle,
//            &output_buffer,
//            &m_Connections[connection_id].m_SSLContext.m_SecAttribs,
//            &m_Connections[connection_id].m_SSLContext.m_SecTimestamp
//            );
//
//          m_Connections[connection_id].m_SSLContext.m_SecHandleInitialized = true;
//        }
//
//        if (return_val == SEC_E_INCOMPLETE_MESSAGE)
//        {
//          PrepareToRecv(connection_id);
//          return;
//        }
//        else
//        {
//          if (return_val == SEC_I_COMPLETE_NEEDED ||
//              return_val == SEC_I_COMPLETE_AND_CONTINUE)
//          {
//            return_val = CompleteAuthToken(&m_Connections[connection_id].m_SSLContext.m_SecHandle, &output_buffer);
//          }
//
//          if (return_val < 0)
//          {
//            m_Allocator.FreeBlockChain(outp_block, StormFixedBlockType::BlockMem);
//
//            SetSocketDisconnected(connection_id);
//            SetDisconnectFlag(connection_id, StormSocketDisconnectFlags::kRecvThread);
//            return;
//          }
//
//          if (return_val != SEC_I_CONTINUE_NEEDED && return_val != SEC_I_COMPLETE_AND_CONTINUE)
//          {
//            m_Connections[connection_id].m_SSLContext.m_SSLHandshakeComplete = true;
//
//            if (QueryContextAttributes(&m_Connections[connection_id].m_SSLContext.m_SecHandle, SECPKG_ATTR_STREAM_SIZES, &m_Connections[connection_id].m_SSLContext.m_StreamSizes) < 0)
//            {
//              throw std::runtime_error("Failed to infer stream sizes");
//            }
//
//            m_HeaderLength = m_HeaderLength < (int)m_Connections[connection_id].m_SSLContext.m_StreamSizes.cbHeader ? 
//                             m_Connections[connection_id].m_SSLContext.m_StreamSizes.cbHeader : m_HeaderLength;
//            m_TrailerLength = m_TrailerLength < (int)m_Connections[connection_id].m_SSLContext.m_StreamSizes.cbHeader ?
//                             m_Connections[connection_id].m_SSLContext.m_StreamSizes.cbTrailer : m_TrailerLength;
//          }
//
//          // Send the output data
//          int output_data_size = output_buffer.cBuffers > 0 ? output_buffer.pBuffers[0].cbBuffer : 0;
//
//          if (output_data_size > 0)
//          {
//            StormMessageWriter writer;
//            writer.m_Allocator = &m_Allocator;
//            writer.m_SenderAllocator = &m_MessageSenders;
//            writer.m_PacketHandle = m_MessageSenders.AllocateBlock(StormFixedBlockType::Sender);
//            writer.m_PacketInfo = (StormMessageWriterData *)m_MessageSenders.ResolveHandle(writer.m_PacketHandle);
//            writer.m_ReservedHeaderLength = 0;
//            writer.m_ReservedTrailerLength = 0;
//            writer.m_HeaderLength = 0;
//            writer.m_TrailerLength = 0;
//            writer.m_Encrypt = false;
//
//            writer.m_PacketInfo->m_StartBlock = outp_block;
//            writer.m_PacketInfo->m_CurBlock = outp_block;
//            writer.m_PacketInfo->m_WriteOffset = output_data_size;
//            writer.m_PacketInfo->m_TotalLength = output_data_size;
//            writer.m_PacketInfo->m_SendOffset = 0;
//            writer.m_PacketInfo->m_RefCount = 1;
//            SendPacketToConnection(writer, connection_id);
//          }
//
//          int extra_data_size = output_buffer.cBuffers > 1 && output_buffer.pBuffers[1].BufferType == SECBUFFER_EXTRA ? output_buffer.pBuffers[1].cbBuffer : 0;
//          m_Connections[connection_id].m_DecryptBuffer.DiscardData(bytes_received - extra_data_size);
//
//          PrepareToRecv(connection_id);
//          return;
//        }
//#endif
//      }
//      else
//      {
//#ifdef USE_WINSEC
//        while (bytes_received > 0)
//        {
//          StormFixedBlockHandle inp_block = m_Connections[connection_id].m_DecryptBuffer.m_BlockStart;
//          int block_size = m_FixedBlockSize - m_Connections[connection_id].m_DecryptBuffer.m_ReadOffset;
//
//          int data_avail = m_Connections[connection_id].m_DecryptBuffer.m_DataAvail;
//          int block_offset = m_Connections[connection_id].m_DecryptBuffer.m_ReadOffset;
//
//          int max_message_size = m_Connections[connection_id].m_SSLContext.m_StreamSizes.cbHeader +
//                                 m_Connections[connection_id].m_SSLContext.m_StreamSizes.cbTrailer +
//                                 m_Connections[connection_id].m_SSLContext.m_StreamSizes.cbMaximumMessage + 2048;
//
//          SecBuffer inp_buffer[128];
//          char * work_buffer = (char *)_alloca(max_message_size);
//          int buffer_size = 0;
//
//          while (inp_block != InvalidBlockHandle && data_avail != 0 && buffer_size < max_message_size)
//          {
//            int data_in_buffer = block_size > data_avail ? data_avail : block_size;
//            int data_to_copy = data_in_buffer + buffer_size < max_message_size ? data_in_buffer : max_message_size - buffer_size;
//            memcpy(&work_buffer[buffer_size], Marshal::MemOffset(m_Allocator.ResolveHandle(inp_block), block_offset), data_to_copy);
//
//            inp_block = m_Allocator.GetNextBlock(inp_block);
//            block_offset = 0;
//            block_size = m_FixedBlockSize;
//            data_avail -= data_to_copy;
//            buffer_size += data_to_copy;
//          }
//
//          int buffer_index = 0;
//          inp_buffer[buffer_index].BufferType = SECBUFFER_DATA;
//          inp_buffer[buffer_index].cbBuffer = buffer_size;
//          inp_buffer[buffer_index].pvBuffer = (char *)work_buffer;
//          buffer_index++;
//
//          for (int index = 0; index < 4; index++)
//          {
//            inp_buffer[buffer_index].BufferType = SECBUFFER_EMPTY;
//            inp_buffer[buffer_index].cbBuffer = 0;
//            inp_buffer[buffer_index].pvBuffer = NULL;
//            buffer_index++;
//          }
//
//          SecBufferDesc input_buffer;
//          input_buffer.cBuffers = buffer_index;
//          input_buffer.ulVersion = SECBUFFER_VERSION;
//          input_buffer.pBuffers = inp_buffer;
//
//          int return_val = DecryptMessage(&m_Connections[connection_id].m_SSLContext.m_SecHandle, &input_buffer, 0, NULL);
//          if (return_val == SEC_E_INCOMPLETE_MESSAGE)
//          {
//            if (buffer_size >= max_message_size)
//            {
//              // This guy is messing with us, just disconnect them
//              SetSocketDisconnected(connection_id);
//              SetDisconnectFlag(connection_id, StormSocketDisconnectFlags::kRecvThread);
//              return;
//            }
//            break;
//          }
//
//          if (return_val < 0)
//          {
//            SetSocketDisconnected(connection_id);
//            SetDisconnectFlag(connection_id, StormSocketDisconnectFlags::kRecvThread);
//            return;
//          }
//
//          if (inp_buffer[1].BufferType == SECBUFFER_DATA)
//          {
//            // Write out the decrypted data to the recv buffer
//            block_offset = m_Connections[connection_id].m_RecvBuffer.m_WriteOffset;
//
//            inp_block = m_Connections[connection_id].m_RecvBuffer.m_BlockCur;
//            block_size = m_FixedBlockSize - block_offset;
//
//            data_avail = inp_buffer[1].cbBuffer;
//
//            char * mem_ptr = (char *)inp_buffer[1].pvBuffer;
//            int data_copied = 0;
//
//            while (inp_block != InvalidBlockHandle && data_avail != 0)
//            {
//              int data_in_buffer = block_size > data_avail ? data_avail : block_size;
//
//              memcpy(Marshal::MemOffset(m_Allocator.ResolveHandle(inp_block), block_offset), &mem_ptr[data_copied], data_in_buffer);
//
//              inp_block = m_Allocator.GetNextBlock(inp_block);
//              block_offset = 0;
//              block_size = m_FixedBlockSize;
//              data_avail -= data_in_buffer;
//              data_copied += data_in_buffer;
//            }
//
//            m_Connections[connection_id].m_RecvBuffer.GotData(data_copied);
//            m_Connections[connection_id].m_UnparsedDataLength.fetch_add(data_copied);
//          }
//
//          // Check for extra data
//          int extra_data_size = input_buffer.cBuffers > 3 && input_buffer.pBuffers[3].BufferType == SECBUFFER_EXTRA ? input_buffer.pBuffers[3].cbBuffer : 0;
//          int data_decrypted = bytes_received - extra_data_size;
//
//          m_Connections[connection_id].m_DecryptBuffer.DiscardData(data_decrypted);
//
//          bytes_received = extra_data_size;
//        }
//#endif
//      }
//    }
//    else
//    {
//      // Data just goes directly into the recv buffer
//      m_Connections[connection_id].m_UnparsedDataLength.fetch_add(bytes_received);
//      m_Connections[connection_id].m_RecvBuffer.GotData(bytes_received);
//    }
//
//    uint64_t prof = Profiling::StartProfiler();
//
//		if (ProcessReceivedData(connection_id) == false)
//		{
//			if (PostQueuedCompletionStatus(m_IoCompletionPort, 0, (ULONG_PTR)connection_id.m_Index.Raw, &m_RecheckOverlap) == FALSE)
//			{
//				throw std::runtime_error("PostQueuedCompletionStatus failed");
//			}
//		}
//		Profiling::EndProfiler(prof, ProfilerCategory::kProcData);
//		PrepareToRecv(connection_id);
//	}
//
//	bool StormSocketServerWin::ProcessReceivedData(StormSocketConnectionId connection_id)
//	{
//		// Lock the receiver
//		int old_val = 0;
//		if (std::atomic_compare_exchange_weak(&m_Connections[connection_id].m_RecvCriticalSection, &old_val, 1) == false)
//		{
//			return false;
//		}
//
//		bool success = ProcessData(connection_id);
//
//		m_Connections[connection_id].m_RecvCriticalSection.store(0);
//		return success;
//	}
//
//	void StormSocketServerWin::PrepareToRecv(StormSocketConnectionId connection_id)
//	{
//    WSABUF buffer_set[2];
//
//    if (m_UseSSL)
//    {
//      void * block_start = m_Allocator.ResolveHandle(m_Connections[connection_id].m_DecryptBuffer.m_BlockCur);
//
//      buffer_set[0].buf = (CHAR *)Marshal::MemOffset(m_Allocator.ResolveHandle(m_Connections[connection_id].m_DecryptBuffer.m_BlockCur), m_Connections[connection_id].m_DecryptBuffer.m_WriteOffset);
//      buffer_set[1].buf = (CHAR *)m_Allocator.ResolveHandle(m_Connections[connection_id].m_DecryptBuffer.m_BlockNext);
//      buffer_set[0].len = m_FixedBlockSize - m_Connections[connection_id].m_DecryptBuffer.m_WriteOffset;
//      buffer_set[1].len = m_FixedBlockSize;
//    }
//    else
//    {
//      void * block_start = m_Allocator.ResolveHandle(m_Connections[connection_id].m_RecvBuffer.m_BlockCur);
//
//      buffer_set[0].buf = (CHAR *)Marshal::MemOffset(m_Allocator.ResolveHandle(m_Connections[connection_id].m_RecvBuffer.m_BlockCur), m_Connections[connection_id].m_RecvBuffer.m_WriteOffset);
//      buffer_set[1].buf = (CHAR *)m_Allocator.ResolveHandle(m_Connections[connection_id].m_RecvBuffer.m_BlockNext);
//      buffer_set[0].len = m_FixedBlockSize - m_Connections[connection_id].m_RecvBuffer.m_WriteOffset;
//      buffer_set[1].len = m_FixedBlockSize;
//    }
//
//
//		DWORD bytes_transfered;
//		DWORD flags = 0;
//		OVERLAPPED * recv_overlap_ptr = &m_RecvOverlaps[connection_id.GetIndex()];
//
//		uint64_t prof = Profiling::StartProfiler();
//		int error = WSARecv(m_Connections[connection_id].m_Socket, buffer_set, 2, &bytes_transfered, &flags, recv_overlap_ptr, NULL);
//		Profiling::EndProfiler(prof, ProfilerCategory::kPrepRecv);
//
//		if (error == SOCKET_ERROR)
//		{
//			error = WSAGetLastError();
//			if (error != WSA_IO_PENDING)
//			{
//				// Disconnect
//				SetSocketDisconnected(connection_id);
//				SetDisconnectFlag(connection_id, StormSocketDisconnectFlags::kRecvThread);
//			}
//		}
//	}
//
//	void StormSocketServerWin::IOThreadMain()
//	{
//		DWORD num_bytes;
//		ULONG_PTR completion_key;
//		LPOVERLAPPED overlapped_ptr;
//
//		while (m_ThreadStopRequested == false)
//		{
//			BOOL success = GetQueuedCompletionStatus(m_IoCompletionPort, &num_bytes, &completion_key, &overlapped_ptr, 1000);
//			int key = (int)completion_key;
//
//			if (success)
//			{
//				if (key == -1)
//				{
//					AcceptNewConnection();
//					continue;
//				}
//
//				if (overlapped_ptr == &m_DisconnectBuffer)
//				{
//					continue;
//				}
//
//				StormSocketConnectionId connection_id;
//				connection_id.m_Index.Raw = key;
//
//				if (connection_id.GetGen() != m_Connections[connection_id.GetIndex()].m_SlotGen)
//				{
//					continue;
//				}
//
//				if (overlapped_ptr == &m_RecheckOverlap)
//				{
//					if (ProcessReceivedData(connection_id) == false)
//					{
//						if (PostQueuedCompletionStatus(m_IoCompletionPort, 0, (ULONG_PTR)connection_id.m_Index.Raw, &m_RecheckOverlap) == FALSE)
//						{
//							throw std::runtime_error("PostQueuedCompletionStatus failed");
//						}
//					}
//
//					continue;
//				}
//
//				if (num_bytes <= 0)
//				{
//					SetSocketDisconnected(connection_id);
//					SetDisconnectFlag(connection_id, StormSocketDisconnectFlags::kRecvThread);
//					continue;
//				}
//
//				if (overlapped_ptr >= m_SendOverlaps.get() && overlapped_ptr <= m_EndSendOverlaps)
//				{
//					SignalOutgoingSocket(connection_id, StormSocketIOOperationType::FreePacket, num_bytes);
//				}
//				else
//				{
//					ProcessNewData(connection_id, num_bytes);
//				}
//			}
//			else if (GetLastError() != WAIT_TIMEOUT)
//			{
//				if (overlapped_ptr != NULL)
//				{
//					if (key != -1)
//					{
//						StormSocketConnectionId connection_id;
//						int key = (int)completion_key;
//						connection_id.m_Index.Raw = key;
//
//						SetSocketDisconnected(connection_id);
//						SetDisconnectFlag(connection_id, StormSocketDisconnectFlags::kRecvThread);
//					}
//				}
//			}
//		}
//	}
//
//	void StormSocketServerWin::SetBufferSet(WSABUF * buffer_set, int buffer_index, void * ptr, int length)
//	{
//		buffer_set[buffer_index].buf = (CHAR *)ptr;
//		buffer_set[buffer_index].len = length;
//	}
//
//	int StormSocketServerWin::FillBufferSet(WSABUF * buffer_set, int & cur_buffer, int pending_data, StormMessageWriter & writer, int send_offset, StormFixedBlockHandle & send_block)
//	{
//		StormFixedBlockHandle block_handle = send_block;
//		send_block = InvalidBlockHandle;
//    int websocket_header_offset = send_offset;
//
//		while (pending_data > 0 && cur_buffer < kBufferSetCount && block_handle != InvalidBlockHandle)
//		{
//      int potential_data_in_block = m_FixedBlockSize - websocket_header_offset - (writer.m_ReservedHeaderLength + writer.m_ReservedTrailerLength);
//			int set_len = min(pending_data, potential_data_in_block);
//      int data_start = writer.m_ReservedHeaderLength - writer.m_HeaderLength + websocket_header_offset;
//      int data_length = writer.m_HeaderLength + set_len + writer.m_TrailerLength;
//
//			void * block = m_Allocator.ResolveHandle(block_handle);
//			SetBufferSet(buffer_set, cur_buffer, Marshal::MemOffset(block, data_start), data_length);
//			block_handle = m_Allocator.GetNextBlock(block_handle);
//
//      websocket_header_offset = 0;
//			pending_data -= set_len;
//			send_block = block_handle;
//
//			cur_buffer++;
//		}
//
//		return pending_data;
//	}
//
//	void StormSocketServerWin::SendThreadMain(int thread_index)
//	{
//		StormSocketIOOperation op;
//
//		WSABUF buffer_set[kBufferSetCount];
//		StormMessageWriter writer;
//
//		while (m_ThreadStopRequested == false)
//		{
//			m_SendThreadSemaphores[thread_index].WaitOne(100);
//
//			while (m_SendQueue[thread_index].TryDequeue(op, 0, m_SendQueueIncdices.get(), m_SendQueueArray.get()))
//			{
//				StormSocketConnectionId connection_id = op.m_ConnectionId;
//				int connection_gen = connection_id.GetGen();
//
//				if (op.m_Type == StormSocketIOOperationType::FreePacket)
//				{
//          m_Connections[connection_id].m_PendingFreeData += op.m_Size;
//					if (connection_gen != m_Connections[op.m_ConnectionId.GetIndex()].m_SlotGen)
//					{
//						continue;
//					}
//
//					StormSocketFreeQueueElement free_elem;
//
//					while (m_FreeQueue[connection_id].PeekTop(free_elem, connection_gen, m_FreeQueueIncdices.get(), m_FreeQueueArray.get(), 0))
//					{
//						int writer_len = free_elem.m_RequestWriter.m_PacketInfo->m_TotalLength;
//						if (m_Connections[connection_id].m_PendingFreeData >= writer_len)
//						{
//							FreeOutgoingPacket(free_elem.m_RequestWriter);
//							ReleasePacketSlot(connection_id);
//
//							m_Connections[connection_id].m_PendingFreeData -= writer_len;
//							m_FreeQueue[connection_id].Advance(connection_gen, m_FreeQueueIncdices.get(), m_FreeQueueArray.get());
//						}
//						else
//						{
//							break;
//						}
//					}
//				}
//				else if (op.m_Type == StormSocketIOOperationType::ClearQueue)
//				{
//          // Clears the connection data, thus freeing this slot from the send thread point of view
//					if (connection_gen != m_Connections[op.m_ConnectionId.GetIndex()].m_SlotGen)
//					{
//						continue;
//					}
//
//					ReleaseSendQueue(connection_id, connection_gen);
//					SetDisconnectFlag(connection_id, StormSocketDisconnectFlags::kSendThread);
//					SignalCloseThread(connection_id);
//				}
//				else if (op.m_Type == StormSocketIOOperationType::Close)
//				{
//					if (connection_gen != m_Connections[op.m_ConnectionId.GetIndex()].m_SlotGen)
//					{
//						continue;
//					}
//
//					SignalCloseThread(connection_id);
//				}
//				else if (op.m_Type == StormSocketIOOperationType::SendPacket)
//				{
//					if (m_OutputQueue[connection_id].PeekTop(writer, connection_gen, m_OutputQueueIncdices.get(), m_OutputQueueArray.get(), 0))
//					{
//						StormSocketFreeQueueElement free_queue_elem;
//
//            if (writer.m_Encrypt && m_UseSSL)
//            {
//              StormMessageWriter encrypted = EncryptWriter(connection_id, writer);
//              m_OutputQueue[connection_id].ReplaceTop(encrypted, connection_gen, m_OutputQueueIncdices.get(), m_OutputQueueArray.get(), 0);
//
//              free_queue_elem.m_RequestWriter = writer;
//              if (m_FreeQueue[connection_id].Enqueue(free_queue_elem, connection_gen, m_FreeQueueIncdices.get(), m_FreeQueueArray.get()) == false)
//              {
//                throw std::runtime_error("Free queue ran out of space");
//              }
//
//              writer = encrypted;
//            }
//
//						int buffer_count = 0;
//						int packet_count = 0;
//						int send_offset = 0;
//						int total_send_length = 0;
//
//						while (buffer_count < kBufferSetCount)
//						{
//							if (m_Connections[connection_id].m_PendingRemainingData == 0)
//							{
//								send_offset = writer.m_PacketInfo->m_SendOffset;
//								m_Connections[connection_id].m_PendingRemainingData = writer.m_PacketInfo->m_TotalLength;
//								m_Connections[connection_id].m_PendingSendBlock = writer.m_PacketInfo->m_StartBlock;
//							}
//
//							int remaining_data =
//								FillBufferSet(buffer_set, buffer_count, m_Connections[connection_id].m_PendingRemainingData, 
//                              writer, send_offset, m_Connections[connection_id].m_PendingSendBlock);
//
//							total_send_length += m_Connections[connection_id].m_PendingRemainingData - remaining_data;
//							m_Connections[connection_id].m_PendingRemainingData = remaining_data;
//
//							packet_count++;
//
//							if (m_OutputQueue[connection_id].PeekTop(writer, connection_gen, m_OutputQueueIncdices.get(), m_OutputQueueArray.get(), packet_count) == false)
//							{
//								break;
//							}
//
//              if (writer.m_Encrypt && m_UseSSL)
//              {
//                StormMessageWriter encrypted = EncryptWriter(connection_id, writer);
//                m_OutputQueue[connection_id].ReplaceTop(encrypted, connection_gen, m_OutputQueueIncdices.get(), m_OutputQueueArray.get(), packet_count);
//
//                free_queue_elem.m_RequestWriter = writer;
//                if (m_FreeQueue[connection_id].Enqueue(free_queue_elem, connection_gen, m_FreeQueueIncdices.get(), m_FreeQueueArray.get()) == false)
//                {
//                  throw std::runtime_error("Free queue ran out of space");
//                }
//
//                writer = encrypted;
//              }
//						}
//
//						int advance_count;
//
//						// If we sent the max blocks worth of data and still have shit to send...
//						if (m_Connections[connection_id].m_PendingRemainingData > 0)
//						{
//							advance_count = packet_count - 1;
//							// Free every writer that got written to except for the last one
//							for (int index = 0; index < packet_count - 1; index++)
//							{
//								m_OutputQueue[connection_id].PeekTop(writer, connection_gen, m_OutputQueueIncdices.get(), m_OutputQueueArray.get(), index);
//								free_queue_elem.m_RequestWriter = writer;
//
//								if (m_FreeQueue[connection_id].Enqueue(free_queue_elem, connection_gen, m_FreeQueueIncdices.get(), m_FreeQueueArray.get()) == false)
//								{
//									throw std::runtime_error("Free queue ran out of space");
//								}
//							}
//							// Requeue up this operation so that we don't block out the number of writers
//							if (m_SendQueue[thread_index].Enqueue(op, 0, m_SendQueueIncdices.get(), m_SendQueueArray.get()) == false)
//							{
//								throw std::runtime_error("Send queue ran out of space");
//							}
//						}
//						else
//						{
//							advance_count = packet_count;
//							// Just free everything
//							for (int index = 0; index < packet_count; index++)
//							{
//								m_OutputQueue[connection_id].PeekTop(writer, connection_gen, m_OutputQueueIncdices.get(), m_OutputQueueArray.get(), index);
//								free_queue_elem.m_RequestWriter = writer;
//
//								if (m_FreeQueue[connection_id].Enqueue(free_queue_elem, connection_gen, m_FreeQueueIncdices.get(), m_FreeQueueArray.get()) == false)
//								{
//									throw std::runtime_error("Free queue ran out of space");
//								}
//							}
//						}
//
//						OVERLAPPED * send_overlap_ptr = &m_SendOverlaps[connection_id];
//
//						uint64_t prof = Profiling::StartProfiler();
//
//						DWORD bytes_transfered;
//						int response_code = WSASend(m_Connections[connection_id].m_Socket, buffer_set, buffer_count,
//							&bytes_transfered, 0, send_overlap_ptr, NULL);
//
//            printf("Sent %d bytes\n", bytes_transfered);
//
//						Profiling::EndProfiler(prof, ProfilerCategory::kSend);
//
//						for (int index = 0; index < advance_count; index++)
//						{
//							m_OutputQueue[connection_id].Advance(connection_gen, m_OutputQueueIncdices.get(), m_OutputQueueArray.get());
//						}
//
//						if (response_code != 0)
//						{
//							int error = WSAGetLastError();
//
//							if (error != (int)ERROR_IO_PENDING)
//							{
//								SetSocketDisconnected(connection_id);
//								break;
//							}
//						}
//					}
//				}
//			}
//		}
//	}
//
//	void StormSocketServerWin::ReleaseSendQueue(StormSocketConnectionId connection_id, int connection_gen)
//	{
//		StormMessageWriter writer;
//		// Lock the queue so that nothing else can put packets into it
//		m_OutputQueue[connection_id].Lock(connection_gen + 1, m_OutputQueueIncdices.get(), m_OutputQueueArray.get());
//
//		// Drain the remaining packets
//		while (m_OutputQueue[connection_id].TryDequeue(writer, connection_gen + 1, m_OutputQueueIncdices.get(), m_OutputQueueArray.get()))
//		{
//			if (writer.m_PacketInfo != NULL)
//			{
//				FreeOutgoingPacket(writer);
//			}
//		}
//
//		m_OutputQueue[connection_id].Reset(connection_gen + 1, m_OutputQueueIncdices.get(), m_OutputQueueArray.get());
//
//		m_FreeQueue[connection_id].Lock(connection_gen + 1, m_FreeQueueIncdices.get(), m_FreeQueueArray.get());
//		StormSocketFreeQueueElement free_elem;
//
//		while (m_FreeQueue[connection_id].TryDequeue(free_elem, connection_gen + 1, m_FreeQueueIncdices.get(), m_FreeQueueArray.get()))
//		{
//			FreeOutgoingPacket(free_elem.m_RequestWriter);
//		}
//
//		m_FreeQueue[connection_id].Reset(connection_gen + 1, m_FreeQueueIncdices.get(), m_FreeQueueArray.get());
//	}
//
//  StormMessageWriter StormSocketServerWin::CreateWriterInternal()
//  {
//    uint64_t prof = Profiling::StartProfiler();
//    StormMessageWriter writer;
//
//#ifdef USE_WINSEC
//    writer.Init(&m_Allocator, &m_MessageSenders, m_UseSSL, m_HeaderLength, m_TrailerLength);
//#else
//    writer.Init(&m_Allocator, &m_MessageSenders, m_UseSSL, 0, 0);
//#endif
//    Profiling::EndProfiler(prof, ProfilerCategory::kCreatePacket);
//    return writer;
//  }
//
//  StormMessageWriter StormSocketServerWin::EncryptWriter(StormSocketConnectionId connection_id, StormMessageWriter & writer)
//  {
//#ifdef USE_WINSEC
//    int header_size = m_Connections[connection_id].m_SSLContext.m_StreamSizes.cbHeader;
//    int trailer_size = m_Connections[connection_id].m_SSLContext.m_StreamSizes.cbTrailer;
//
//    StormMessageWriter dupe = writer.Duplicate();
//    dupe.m_Encrypt = false;
//    dupe.m_HeaderLength = header_size;
//    dupe.m_TrailerLength = trailer_size;
//    StormFixedBlockHandle cur_block = dupe.m_PacketInfo->m_StartBlock;
//
//    int data_to_encrypt = dupe.m_PacketInfo->m_TotalLength;
//
//    int block_index = 0;
//    while (cur_block != InvalidBlockHandle)
//    {
//      void * block_base = m_Allocator.ResolveHandle(cur_block);
//      int start_offset = (block_index == 0 ? dupe.m_PacketInfo->m_SendOffset : 0);
//      int header_offset = dupe.m_ReservedHeaderLength - header_size + start_offset;
//      int block_size = dupe.m_Allocator->GetBlockSize() - (dupe.m_ReservedHeaderLength + dupe.m_ReservedTrailerLength + start_offset);
//
//      SecBuffer buffers[4];
//      buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
//      buffers[0].cbBuffer = header_size;
//      buffers[0].pvBuffer = Marshal::MemOffset(block_base, header_offset);
//
//      buffers[1].BufferType = SECBUFFER_DATA;
//      buffers[1].cbBuffer = block_size < data_to_encrypt ? block_size : data_to_encrypt;
//      buffers[1].pvBuffer = Marshal::MemOffset(block_base, header_offset + header_size);
//
//      buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
//      buffers[2].cbBuffer = trailer_size;
//      buffers[2].pvBuffer = Marshal::MemOffset(buffers[1].pvBuffer, buffers[1].cbBuffer);
//
//      buffers[3].BufferType = SECBUFFER_EMPTY;
//      buffers[3].cbBuffer = 0;
//      buffers[3].pvBuffer = NULL;
//
//      SecBufferDesc buffer_desc;
//      buffer_desc.ulVersion = SECBUFFER_VERSION;
//      buffer_desc.cBuffers = 4;
//      buffer_desc.pBuffers = buffers;
//
//      int return_val = EncryptMessage(&m_Connections[connection_id].m_SSLContext.m_SecHandle, 0, &buffer_desc, 0);
//      if (return_val < 0)
//      {
//        throw std::runtime_error("Failed to encrypt ssl message");
//      }
//
//      cur_block = m_Allocator.GetNextBlock(cur_block);
//      block_index++;
//    }
//
//    return dupe;
//#endif
//
//    return writer;
//  }
//
//  void StormSocketServerWin::CloseSocket(StormSocketConnectionId id)
//  {
//    CloseSocketInternal(m_Connections[id].m_Socket);
//  }
//
//	void StormSocketServerWin::CloseSocketInternal(int socket)
//	{
//		DisconnectEx(socket, &m_DisconnectBuffer, 0, 0);
//	}
//
//  void StormSocketServerWin::FreeConnectionResources(StormSocketConnectionId id)
//  {
//    StormSocketServerFrontendBase::FreeConnectionResources(id);
//
//#ifdef USE_WINSEC
//    if (m_UseSSL && m_Connections[id].m_SSLContext.m_SecHandleInitialized)
//    {
//      DeleteSecurityContext(&m_Connections[id].m_SSLContext.m_SecHandle);
//    }
//#endif
//  }
//}
//
//#endif