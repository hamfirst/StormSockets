#pragma once

#include "StormSocketFrontendHttpBase.h"
#include "StormSocketConnectionHttp.h"
#include "StormHttpHeaderValues.h"
#include "StormHttpResponseWriter.h"
#include "StormSha1.h"

namespace StormSockets
{
  class StormSocketServerFrontendHttp : public StormSocketFrontendHttpBase
  {
  protected:
    StormFixedBlockAllocator m_ConnectionAllocator;
    StormSocketServerSSLData m_SSLData;
    bool m_UseSSL;

    StormSocketBackendAcceptorId m_AcceptorId;

    StormHttpHeaderValues m_HeaderValues;

  public:

    StormSocketServerFrontendHttp(const StormSocketServerFrontendHttpSettings & settings, StormSocketBackend * backend);
    ~StormSocketServerFrontendHttp();

#ifdef USE_MBED
    bool UseSSL(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id) { return m_UseSSL; }
    mbedtls_ssl_config * GetSSLConfig() { return &m_SSLData.m_SSLConfig; }
#endif

    StormHttpResponseWriter CreateOutgoingResponse(int response_code, char * response_phrase);
    void FinalizeOutgoingResponse(StormHttpResponseWriter & writer, bool write_content_length);
    void SendResponse(StormSocketConnectionId connection_id, StormHttpResponseWriter & writer);
    void FreeOutgoingResponse(StormHttpResponseWriter & writer);

  protected:

    StormSocketServerConnectionHttp & GetHttpConnection(StormSocketFrontendConnectionId id);

    StormSocketFrontendConnectionId AllocateFrontendId();
    void FreeFrontendId(StormSocketFrontendConnectionId frontend_id);

    void InitConnection(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id, const void * init_data);
    void CleanupConnection(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);
    bool ProcessData(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);
    bool ParseRequestLine(StormMessageReaderCursor & request_line, StormSocketServerConnectionHttp & http_connection);

    void AddBodyBlock(StormSocketConnectionId connection_id, StormHttpConnectionBase & http_connection, void * chunk_ptr, int chunk_len, int read_offset);
    bool CompleteBody(StormSocketConnectionId connection_id, StormHttpConnectionBase & http_connection);

    void SendClosePacket(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id);
    void ConnectionEstablishComplete(StormSocketConnectionId connection_id, StormSocketFrontendConnectionId frontend_id) { }
  };
}

