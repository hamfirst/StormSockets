
#pragma once


#include "StormSocketBackend.h"
#include "StormSocketFrontend.h"
#include "StormFixedBlockAllocator.h"
#include "StormMessageQueue.h"
#include "StormSocketServerTypes.h"
#include "StormSocketIOOperation.h"

#include <thread>
#include <unordered_set>
#include <memory>
#include <vector>

namespace StormSockets
{
  struct StormSocketServerSSLData
  {
#ifndef DISABLE_MBED

    mbedtls_x509_crt m_Cert;
    mbedtls_pk_context m_PrivateKey;

    mbedtls_entropy_context m_Entropy;
    mbedtls_ctr_drbg_context m_CtrDrbg;

    mbedtls_ssl_config m_SSLConfig;

#endif
  };

  struct StormSocketClientSSLData
  {
#ifndef DISABLE_MBED
    mbedtls_x509_crt m_CA;
    mbedtls_entropy_context m_Entropy;
    mbedtls_ctr_drbg_context m_CtrDrbg;

    mbedtls_ssl_config m_SSLConfig;
#endif
  };

	class StormSocketFrontendBase : public StormSocketFrontend
	{
	protected:
		// Allocator for send and recv buffers.  These buffers interact directly with the low level api
		StormFixedBlockAllocator & m_Allocator;

		// Allocator for message senders.  These store data about packets as they are being built by external code
		StormFixedBlockAllocator & m_MessageSenders;

		// Allocator for message readers.  These store data about packets as they are being read by external code
		StormFixedBlockAllocator & m_MessageReaders;

    int m_FixedBlockSize;
		int m_MaxConnections;

		// Queue that stores event data which is consumed by external code.  Events tell the user there was a connect or disconnect or new packet
		StormMessageQueue<StormSocketEventInfo> m_EventQueue;
    StormSocketBackend * m_Backend;

    std::unordered_set<StormSocketConnectionId> m_OwnedConnections;
    std::mutex m_OwnedConnectionMutex;
    std::unique_lock<std::mutex> m_OwnedConnectionLock;

    StormSemaphore * m_EventSemaphore;

	public:
    StormSocketFrontendBase(const StormSocketFrontendSettings & settings, StormSocketBackend * backend);

		bool GetEvent(StormSocketEventInfo & message);

		bool SendPacketToConnection(StormMessageWriter & writer, StormSocketConnectionId id);
		void SendPacketToConnectionBlocking(StormMessageWriter & writer, StormSocketConnectionId id);
		void FreeOutgoingPacket(StormMessageWriter & writer);

		void FinalizeConnection(StormSocketConnectionId id);
		void ForceDisconnect(StormSocketConnectionId id);

    void MemoryAudit();
	protected:

    void AssociateConnectionId(StormSocketConnectionId connection_id);
    void DisassociateConnectionId(StormSocketConnectionId connection_id);
    void CleanupAllConnections();

    bool InitServerSSL(const StormSocketServerSSLSettings & ssl_settings, StormSocketServerSSLData & ssl_data);
    void ReleaseServerSSL(StormSocketServerSSLData & ssl_data);

    void InitClientSSL(StormSocketClientSSLData & ssl_data, StormSocketBackend * backend);
    void ReleaseClientSSL(StormSocketClientSSLData & ssl_data);


    void QueueConnectEvent(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id, uint32_t remote_ip, uint16_t remote_port);
    void QueueHandshakeCompleteEvent(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);
    void QueueDisconnectEvent(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);
    void ConnectionEstablishComplete(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);
    StormSocketConnectionBase & GetConnection(int index);
	};
}