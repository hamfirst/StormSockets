#pragma once

#include "StormSocketFrontendBase.h"
#include "StormSocketConnectionHttp.h"

namespace StormSockets
{
  class StormSocketFrontendHttpBase : public StormSocketFrontendBase
  {
  protected:

    StormSocketFrontendHttpBase(const StormSocketFrontendHttpSettings & settings, StormSocketBackend * backend);
    bool ProcessHttpData(StormSocketConnectionBase & connection, StormHttpConnectionBase & http_connection, StormSocketConnectionId connection_id);

    virtual void AddBodyBlock(StormSocketConnectionId connection_id, StormHttpConnectionBase & http_connection, void * chunk_ptr, int chunk_len, int read_offset) = 0;
    virtual bool CompleteBody(StormSocketConnectionId connection_id, StormHttpConnectionBase & http_connection) = 0;

    void DiscardParsedData(StormSocketConnectionId connection_id, StormHttpConnectionBase & http_connection, int amount);

  };
}
