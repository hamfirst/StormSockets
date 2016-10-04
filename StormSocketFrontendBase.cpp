
#include "StormSocketFrontendBase.h"

#include <fstream>

#ifdef USE_MBED

#include <sspi.h>
#include <schnlsp.h>
#include <ntsecapi.h>

#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Secur32.lib")

#define MBED_CHECK_ERROR if(error < 0) throw std::runtime_error("Certificate load error " + std::to_string(error));
#endif

namespace StormSockets
{
  StormSocketFrontendBase::StormSocketFrontendBase(const StormSocketFrontendSettings & settings, StormSocketBackend * backend) :
    m_Allocator(backend->GetAllocator()),
    m_MessageSenders(backend->GetMessageSenders()),
    m_MessageReaders(backend->GetMessageReaders()),
    m_EventQueue(settings.MessageQueueSize),
    m_Backend(backend),
    m_OwnedConnectionLock(m_OwnedConnectionMutex, std::defer_lock_t{})
  {
    m_MaxConnections = settings.MaxConnections;

    m_FixedBlockSize = backend->GetFixedBlockSize();
    m_MaxPendingFrees = backend->GetMaxPendingFrees();

    m_OwnedConnections.reserve(settings.MaxConnections);
  }

  void StormSocketFrontendBase::WaitForEvent(int timeout_ms)
  {
    std::unique_lock<std::mutex> lock(m_EventMutex);
    m_EventCondition.wait_for(lock, std::chrono::milliseconds(timeout_ms));
  }

  bool StormSocketFrontendBase::GetEvent(StormSocketEventInfo & message)
  {
    if (m_EventQueue.TryDequeue(message))
    {
      return true;
    }

    return false;
  }

  bool StormSocketFrontendBase::SendPacketToConnection(StormMessageWriter & writer, StormSocketConnectionId id)
  {
    return m_Backend->SendPacketToConnection(writer, id);
  }

  void StormSocketFrontendBase::SendPacketToConnectionBlocking(StormMessageWriter & writer, StormSocketConnectionId id)
  {
    m_Backend->SendPacketToConnectionBlocking(writer, id);
  }

  void StormSocketFrontendBase::FreeOutgoingPacket(StormMessageWriter & writer)
  {
    m_Backend->FreeOutgoingPacket(writer);
  }

  void StormSocketFrontendBase::FinalizeConnection(StormSocketConnectionId id)
  {
    m_Backend->FinalizeConnection(id);
  }

  void StormSocketFrontendBase::ForceDisconnect(StormSocketConnectionId id)
  {
    m_Backend->SetDisconnectFlag(id, StormSocketDisconnectFlags::kLocalClose);
    m_Backend->SetDisconnectFlag(id, StormSocketDisconnectFlags::kRemoteClose);
  }

  void StormSocketFrontendBase::AssociateConnectionId(StormSocketConnectionId connection_id)
  {
    if (m_OwnedConnectionLock.owns_lock())
    {
      m_OwnedConnections.insert(connection_id);
    }
    else
    {
      std::lock_guard<std::mutex> guard(m_OwnedConnectionMutex);
      m_OwnedConnections.insert(connection_id);
    }
  }

  void StormSocketFrontendBase::DisassociateConnectionId(StormSocketConnectionId connection_id)
  {
    if (m_OwnedConnectionLock.owns_lock())
    {
      m_OwnedConnections.erase(connection_id);
    }
    else
    {
      std::lock_guard<std::mutex> guard(m_OwnedConnectionMutex);
      m_OwnedConnections.erase(connection_id);
    }
  }

  void StormSocketFrontendBase::CleanupAllConnections()
  {
    m_OwnedConnectionLock.lock();
    for (auto connection_id : m_OwnedConnections)
    {
      ForceDisconnect(connection_id);
      FinalizeConnection(connection_id);
    }
    m_OwnedConnectionLock.unlock();

    while (true)
    {
      m_OwnedConnectionLock.lock();
      if (m_OwnedConnections.size() == 0)
      {
        return;
      }
      m_OwnedConnectionLock.unlock();

      std::this_thread::yield();
    }
  }

  bool StormSocketFrontendBase::InitServerSSL(const StormSocketServerSSLSettings & ssl_settings, StormSocketServerSSLData & ssl_data)
  {
    if (ssl_settings.CertificateFile && ssl_settings.PrivateKeyFile != nullptr)
    {
      std::ifstream cert_file;
      cert_file.open(ssl_settings.CertificateFile, std::ifstream::in | std::ifstream::binary);
      cert_file.seekg(0, std::ios::end);
      int cert_file_length = (int)cert_file.tellg();
      cert_file.seekg(0, std::ios::beg);

      std::unique_ptr<char[]> cert_data = std::make_unique<char[]>(cert_file_length + 1);
      cert_file.read(cert_data.get(), cert_file_length);
      cert_data[cert_file_length] = 0;
      cert_file.close();

      std::ifstream key_file;
      key_file.open(ssl_settings.PrivateKeyFile, std::ifstream::in | std::ifstream::binary);
      key_file.seekg(0, std::ios::end);
      int key_file_length = (int)key_file.tellg();
      key_file.seekg(0, std::ios::beg);

      std::unique_ptr<char[]> key_data = std::make_unique<char[]>(key_file_length + 1);
      key_file.read(key_data.get(), key_file_length);
      key_data[key_file_length] = 0;
      key_file.close();

#ifdef USE_MBED

      int error;

      mbedtls_x509_crt_init(&ssl_data.m_Cert);
      error = mbedtls_x509_crt_parse(&ssl_data.m_Cert, (uint8_t *)cert_data.get(), cert_file_length + 1); MBED_CHECK_ERROR;

      mbedtls_pk_init(&ssl_data.m_PrivateKey);
      error = mbedtls_pk_parse_key(&ssl_data.m_PrivateKey, (uint8_t *)key_data.get(), key_file_length + 1, nullptr, 0); MBED_CHECK_ERROR;

      mbedtls_entropy_init(&ssl_data.m_Entropy);
      mbedtls_ctr_drbg_init(&ssl_data.m_CtrDrbg);

      const char *pers = "stormsockets";
      error = mbedtls_ctr_drbg_seed(&ssl_data.m_CtrDrbg, mbedtls_entropy_func, &ssl_data.m_Entropy, (uint8_t *)pers, strlen(pers)); MBED_CHECK_ERROR;

      mbedtls_ssl_config_init(&ssl_data.m_SSLConfig);

      error = mbedtls_ssl_config_defaults(&ssl_data.m_SSLConfig,
        MBEDTLS_SSL_IS_SERVER,
        MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT);
      MBED_CHECK_ERROR;

      mbedtls_ssl_conf_rng(&ssl_data.m_SSLConfig, mbedtls_ctr_drbg_random, &ssl_data.m_CtrDrbg);

      auto debug_func = [](void *ctx, int level, const char *file, int line, const char *str)
      {
        fprintf((FILE *)ctx, "%s:%04d: %s", file, line, str);
        fflush((FILE *)ctx);
      };

      mbedtls_ssl_conf_dbg(&ssl_data.m_SSLConfig, debug_func, stdout);

      mbedtls_ssl_conf_ca_chain(&ssl_data.m_SSLConfig, ssl_data.m_Cert.next, NULL);
      error = mbedtls_ssl_conf_own_cert(&ssl_data.m_SSLConfig, &ssl_data.m_Cert, &ssl_data.m_PrivateKey); MBED_CHECK_ERROR;

      //mbedtls_debug_set_threshold(5);

#endif

      return true;
    }

    return false;
  }

  void StormSocketFrontendBase::ReleaseServerSSL(StormSocketServerSSLData & ssl_data)
  {
#ifdef USE_MBED
    mbedtls_x509_crt_free(&ssl_data.m_Cert);
    mbedtls_pk_free(&ssl_data.m_PrivateKey);
    mbedtls_ssl_config_free(&ssl_data.m_SSLConfig);
    mbedtls_ctr_drbg_free(&ssl_data.m_CtrDrbg);
    mbedtls_entropy_free(&ssl_data.m_Entropy);
#endif
  }

  void StormSocketFrontendBase::InitClientSSL(StormSocketClientSSLData & ssl_data)
  {
#ifdef USE_MBED
    mbedtls_entropy_init(&ssl_data.m_Entropy);
    mbedtls_ctr_drbg_init(&ssl_data.m_CtrDrbg);

    const char *pers = "stormsockets";
    auto error = mbedtls_ctr_drbg_seed(&ssl_data.m_CtrDrbg, mbedtls_entropy_func, &ssl_data.m_Entropy, (uint8_t *)pers, strlen(pers)); MBED_CHECK_ERROR;

    mbedtls_ssl_config_init(&ssl_data.m_SSLConfig);

    error = mbedtls_ssl_config_defaults(&ssl_data.m_SSLConfig,
      MBEDTLS_SSL_IS_CLIENT,
      MBEDTLS_SSL_TRANSPORT_STREAM,
      MBEDTLS_SSL_PRESET_DEFAULT);
    MBED_CHECK_ERROR;

    mbedtls_ssl_conf_rng(&ssl_data.m_SSLConfig, mbedtls_ctr_drbg_random, &ssl_data.m_CtrDrbg);

    auto debug_func = [](void *ctx, int level, const char *file, int line, const char *str)
    {
      fprintf((FILE *)ctx, "%s:%04d: %s", file, line, str);
      fflush((FILE *)ctx);
    };

    mbedtls_x509_crt_init(&ssl_data.m_CA);
#ifdef _WINDOWS

    auto cert_store = CertOpenSystemStore(NULL, L"ROOT");
    PCCERT_CONTEXT cert_context = nullptr;

    while ((cert_context = CertEnumCertificatesInStore(cert_store, cert_context)) != nullptr)
    {
      if ((cert_context->dwCertEncodingType & X509_ASN_ENCODING) != 0)
      {
        mbedtls_x509_crt_parse(&ssl_data.m_CA, cert_context->pbCertEncoded, cert_context->cbCertEncoded);
      }
    }

    CertCloseStore(cert_store, 0);
#endif

    mbedtls_ssl_conf_ca_chain(&ssl_data.m_SSLConfig, &ssl_data.m_CA, nullptr);

    mbedtls_ssl_conf_dbg(&ssl_data.m_SSLConfig, debug_func, stdout);
    mbedtls_ssl_conf_authmode(&ssl_data.m_SSLConfig, MBEDTLS_SSL_VERIFY_REQUIRED);
#endif
  }

  void StormSocketFrontendBase::ReleaseClientSSL(StormSocketClientSSLData & ssl_data)
  {
#ifdef USE_MBED
    mbedtls_x509_crt_free(&ssl_data.m_CA);
    mbedtls_ssl_config_free(&ssl_data.m_SSLConfig);
    mbedtls_ctr_drbg_free(&ssl_data.m_CtrDrbg);
    mbedtls_entropy_free(&ssl_data.m_Entropy);
#endif
  }

  void StormSocketFrontendBase::QueueConnectEvent(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id, uint32_t remote_ip, uint16_t remote_port)
  {
    // Queue up the information about the new connection
    StormSocketEventInfo connect_message;
    connect_message.ConnectionId = connection_id;
    connect_message.Type = StormSocketEventType::ClientConnected;
    connect_message.RemoteIP = remote_ip;
    connect_message.RemotePort = remote_port;
    while (m_EventQueue.Enqueue(connect_message) == false)
    {
      std::this_thread::yield();
    }

    m_EventCondition.notify_one();
  }

  void StormSocketFrontendBase::QueueHandshakeCompleteEvent(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id)
  {
    auto & connection = GetConnection(connection_id);

    // Queue up the information about the new connection
    StormSocketEventInfo connect_message;
    connect_message.ConnectionId = connection_id;
    connect_message.Type = StormSocketEventType::ClientHandShakeCompleted;
    connect_message.RemoteIP = connection.m_RemoteIP;
    connect_message.RemotePort = connection.m_RemotePort;
    while (m_EventQueue.Enqueue(connect_message) == false)
    {
      std::this_thread::yield();
    }

    m_EventCondition.notify_one();
  }

  void StormSocketFrontendBase::QueueDisconnectEvent(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id)
  {
    auto & connection = m_Backend->GetConnection(connection_id);
    // Tell the main thread that we've disconnected
    StormSocketEventInfo disconnect_message;
    disconnect_message.ConnectionId = connection_id;
    disconnect_message.Type = StormSocketEventType::Disconnected;
    disconnect_message.RemoteIP = connection.m_RemoteIP;
    disconnect_message.RemotePort = connection.m_RemotePort;

    while (m_EventQueue.Enqueue(disconnect_message) == false)
    {
      std::this_thread::yield();
    }

    m_EventCondition.notify_one();
  }

  void StormSocketFrontendBase::ConnectionEstablishComplete(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id)
  {
    QueueHandshakeCompleteEvent(connection_id, frontend_id);
  }

  StormSocketConnectionBase & StormSocketFrontendBase::GetConnection(int index)
  {
    return m_Backend->GetConnection(index);
  }
}
