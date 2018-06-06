#pragma once

#include "StormFixedBlockAllocator.h"
#include "StormSocketBuffer.h"
#include "StormMessageWriter.h"
#include "StormWebsocketMessageReader.h"

#ifndef DISABLE_MBED
#include <mbedtls/ssl.h>
#endif

namespace StormSockets
{
	namespace StormSocketDisconnectFlags
	{
		enum Index
		{
			kMainThread = 0x0001,
			kRecvThread = 0x0002,
			kSendThread = 0x0004,
			kLocalClose = 0x0008,
			kRemoteClose = 0x0010,
			kThreadClose = 0x0020,
      kConnectFinished = 0x0040,
      kTerminateFlags = kMainThread | kRecvThread | kSendThread | kLocalClose | kRemoteClose | kThreadClose,
			kAllFlags = kMainThread | kRecvThread | kSendThread | kLocalClose | kRemoteClose | kThreadClose | kConnectFinished,
			kCloseFlags = kRemoteClose | kLocalClose,
			kSocket = 0x0080,
			kSignalClose = 0x0100,
		};
	}

  struct SSLContext
  {
    SSLContext()
    {
      memset(this, 0, sizeof(SSLContext));
    }

#ifndef DISABLE_MBED
    mbedtls_ssl_context m_SSLContext;
#endif

    bool m_SecHandleInitialized;
    bool m_SSLHandshakeComplete;
  };

  using StormSocketFrontendConnectionId = StormFixedBlockHandle;

  class StormSocketFrontend;

  struct StormSocketConnectionBase
  {
    std::atomic_flag m_Used = ATOMIC_FLAG_INIT;
    unsigned int m_RemoteIP = 0;
    unsigned short m_RemotePort = 0;
    StormSocketFrontend * m_Frontend = nullptr;
    StormSocketFrontendConnectionId m_FrontendId = InvalidBlockHandle;

    StormSocketBuffer m_RecvBuffer;
    StormSocketBuffer m_DecryptBuffer;
    StormFixedBlockHandle m_ParseBlock;
    std::atomic_int m_UnparsedDataLength;
    int m_ParseOffset = 0;
    int m_ReadOffset = 0;
    int m_DisconnectFlags = 0;
    std::atomic_int m_PendingPackets;
    volatile int m_SlotGen = 0;
    int m_SlotIndex = 0;
    std::atomic_int m_RecvCriticalSection;

    StormFixedBlockHandle m_PendingSendBlockStart;
    StormFixedBlockHandle m_PendingSendBlockCur;
    std::atomic_bool m_Transmitting;

    SSLContext m_SSLContext = {};

#ifndef DISABLE_MBED
    StormMessageWriter m_EncryptWriter = {};
#endif

    std::atomic_int m_PacketsSent;
    std::atomic_int m_PacketsRecved;

    std::mutex m_TimeoutLock;
    std::atomic_bool m_HandshakeComplete;

    volatile bool m_Allocated = false;
    volatile bool m_FailedConnection = false;
    volatile bool m_Closing = false;
  };
}