#pragma once

#include "StormSocketFrontendHttpBase.h"
#include "StormSocketConnectionHttp.h"
#include "StormHttpHeaderValues.h"
#include "StormHttpRequestWriter.h"
#include "StormSocketRequest.h"
#include "StormUrlUtil.h"
#include "StormSha1.h"

namespace StormSockets
{
  class StormSocketClientFrontendHttp : public StormSocketFrontendHttpBase
  {
  protected:
    StormFixedBlockAllocator m_ConnectionAllocator;
    std::unique_ptr<StormSocketClientSSLData[]> m_SSLData;

    StormHttpHeaderValues m_HeaderValues;
  public:

    StormSocketClientFrontendHttp(const StormSocketClientFrontendHttpSettings & settings, StormSocketBackend * backend);
    ~StormSocketClientFrontendHttp();

    StormSocketConnectionId RequestConnect(const char * ip_addr, int port, const StormSocketClientFrontendHttpRequestData & request_data);
    StormSocketConnectionId RequestConnect(const StormURI & uri, const void * body, int body_len, const void * headers, int header_len);
    StormSocketConnectionId RequestConnect(const char * url, const void * body, int body_len, const void * headers, int header_len);
    StormSocketConnectionId RequestConnect(const StormURI & uri, const char * method, const void * body, int body_len, const void * headers, int header_len);

    void FreeIncomingHttpResponse(StormHttpResponseReader & reader);
    void MemoryAudit();

#ifndef DISABLE_MBED
    bool UseSSL(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);
    mbedtls_ssl_config * GetSSLConfig(StormSocketFrontendConnectionId frontend_id);
#endif

  protected:

    StormSocketClientConnectionHttp & GetHttpConnection(StormSocketFrontendConnectionId id);

    StormSocketFrontendConnectionId AllocateFrontendId();
    void FreeFrontendId(StormSocketFrontendConnectionId frontend_id);

    void InitConnection(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id, const void * init_data);
    void CleanupConnection(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);

    void QueueDisconnectEvent(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id) override;

    bool ProcessData(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);
    bool ParseStatusLine(StormMessageReaderCursor & status_line, StormSocketClientConnectionHttp & http_connection);

    void ConnectionEstablishComplete(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);

    void AddBodyBlock(StormSocketConnectionId connection_id, StormHttpConnectionBase & http_connection, void * chunk_ptr, int chunk_len, int read_offset);
    bool CompleteBody(StormSocketConnectionId connection_id, StormHttpConnectionBase & http_connection);

    void SendClosePacket(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);
  };
}

