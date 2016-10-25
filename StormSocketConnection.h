#pragma once

#include "StormFixedBlockAllocator.h"
#include "StormSocketBuffer.h"
#include "StormMessageWriter.h"
#include "StormWebsocketMessageReader.h"

#ifdef USE_MBED
#include <mbedtls\ssl.h>
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

#ifdef USE_MBED
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
    unsigned int m_RemoteIP;
    unsigned short m_RemotePort;
    StormSocketFrontend * m_Frontend;
    StormSocketFrontendConnectionId m_FrontendId;

    StormSocketBuffer m_RecvBuffer;
    StormSocketBuffer m_DecryptBuffer;
    StormFixedBlockHandle m_ParseBlock;
    StormFixedBlockHandle m_PendingSendBlock;
    std::atomic_int m_UnparsedDataLength;
    int m_ParseOffset;
    int m_ReadOffset;
    int m_DisconnectFlags;
    std::atomic_int m_PendingPackets;
    int m_SlotGen;
    int m_PendingRemainingData;
    int m_PendingFreeData;
    std::atomic_int m_RecvCriticalSection;

    SSLContext m_SSLContext;

#ifdef USE_MBED
    StormMessageWriter m_EncryptWriter;
#endif


    std::atomic_int m_PacketsSent;
    std::atomic_int m_PacketsRecved;

    volatile bool m_FailedConnection;
  };
}