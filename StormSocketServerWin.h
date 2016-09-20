//
//#pragma once
//
//#ifdef _WINDOWS
//
//#include <WinSock2.h>
//#include <MSWSock.h>
//#include <Ws2tcpip.h>
//#include <Wincrypt.h>
//
//#include <StormSockets\StormSocketServerFrontendBase.h>
//
//namespace StormSockets
//{
//	class StormSocketServerWin : public StormSocketServerFrontendBase
//	{
//		HANDLE m_IoCompletionPort;
//		int m_ListenSocket;
//
//		int m_AcceptSocket;
//		uint8_t m_AcceptBuffer[sizeof(sockaddr_in) * 2 + 16];
//		OVERLAPPED m_DisconnectBuffer;
//
//		std::unique_ptr<std::thread[]> m_IOThreads;
//    std::unique_ptr<std::thread[]> m_SendThreads;
//
//		OVERLAPPED m_AcceptOverlap;
//		OVERLAPPED m_RecheckOverlap;
//    std::unique_ptr<OVERLAPPED[]> m_SendOverlaps;
//    std::unique_ptr<OVERLAPPED[]> m_RecvOverlaps;
//    OVERLAPPED * m_EndSendOverlaps;
//
//		uint32_t m_LocalInterface;
//		uint16_t m_Port;
//
//		LPFN_DISCONNECTEX DisconnectEx;
//
//#ifdef USE_WINSEC
//    PCCERT_CONTEXT m_CertContext;
//    SecHandle m_ServerSSLContext;
//    
//    int m_HeaderLength;
//    int m_TrailerLength;
//#endif
//
//		static const int kBufferSetCount = 8;
//	public:
//
//		StormSocketServerWin(StormSocketInitSettings & settings);
//		~StormSocketServerWin();
//
//		void Start();
//	private:
//
//		void PrepareToAccept();
//		void AcceptNewConnection();
//
//		void ProcessNewData(StormSocketConnectionId connection_id, int bytes_received);
//		bool ProcessReceivedData(StormSocketConnectionId connection_id);
//		void PrepareToRecv(StormSocketConnectionId connection_id);
//
//		void IOThreadMain();
//		void SetBufferSet(WSABUF * buffer_set, int buffer_index, void * ptr, int length);
//
//		int FillBufferSet(WSABUF * buffer_set, int & cur_buffer, int pending_data, StormMessageWriter & writer, int send_offset, StormFixedBlockHandle & send_block);
//
//		void SendThreadMain(int thread_index);
//
//		void ReleaseSendQueue(StormSocketConnectionId connection_id, int connection_gen);
//    StormMessageWriter CreateWriterInternal();
//    StormMessageWriter EncryptWriter(StormSocketConnectionId connection_id, StormMessageWriter & writer);
//
//    void CloseSocket(StormSocketConnectionId id);
//		void CloseSocketInternal(int socket);
//    void FreeConnectionResources(StormSocketConnectionId id);
//	};
//}
//
//#endif