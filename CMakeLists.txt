cmake_minimum_required(VERSION 3.0)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/.. ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/External)

set(CMAKE_CXX_STANDARD 17)

set(SRC_StormSocketCPP
            ./StormFixedBlockAllocator.cpp
            ./StormHttpBodyReader.cpp
            ./StormHttpHeaderValues.cpp
            ./StormHttpRequestReader.cpp
            ./StormHttpRequestWriter.cpp
            ./StormHttpResponseReader.cpp
            ./StormHttpResponseWriter.cpp
            ./StormMessageHeaderReader.cpp
            ./StormMessageHeaderValues.cpp
            ./StormMessageReaderCursor.cpp
            ./StormMessageReaderUtil.cpp
            ./StormMessageWriter.cpp
            ./StormProfiling.cpp
            ./StormSemaphore.cpp
            ./StormSha1.cpp
            ./StormSocketBackend.cpp
            ./StormSocketBuffer.cpp
            ./StormSocketClientFrontendHttp.cpp
            ./StormSocketClientFrontendWebsocket.cpp
            ./StormSocketConnectionId.cpp
            ./StormSocketFrontendBase.cpp
            ./StormSocketFrontendHttpBase.cpp
            ./StormSocketFrontendWebsocketBase.cpp
            ./StormSocketLog.cpp
            ./StormSocketServerFrontendHttp.cpp
            ./StormSocketServerFrontendWebsocket.cpp
            ./StormSocketServerWebsocket.cpp
            ./StormSocketServerWin.cpp
            ./StormUrlUtil.cpp
            ./StormWebsocketHeaderValues.cpp
            ./StormWebsocketMessageReader.cpp
            ./StormWebsocketMessageWriter.cpp
            )
set(HEADER_StormSocketCPP
            ./StormFixedBlockAllocator.h
            ./StormGenIndex.h
            ./StormHttpBodyReader.h
            ./StormHttpHeaderValues.h
            ./StormHttpRequestReader.h
            ./StormHttpRequestWriter.h
            ./StormHttpResponseReader.h
            ./StormHttpResponseWriter.h
            ./StormMemOps.h
            ./StormMessageHeaderReader.h
            ./StormMessageHeaderValues.h
            ./StormMessageQueue.h
            ./StormMessageReaderCursor.h
            ./StormMessageReaderData.h
            ./StormMessageReaderUtil.h
            ./StormMessageWriter.h
            ./StormMutex.h
            ./StormProfiling.h
            ./StormSemaphore.h
            ./StormSha1.h
            ./StormSocketBackend.h
            ./StormSocketBuffer.h
            ./StormSocketClientFrontendHttp.h
            ./StormSocketClientFrontendWebsocket.h
            ./StormSocketConnection.h
            ./StormSocketConnectionHttp.h
            ./StormSocketConnectionId.h
            ./StormSocketConnectionWebsocket.h
            ./StormSocketFrontend.h
            ./StormSocketFrontendBase.h
            ./StormSocketFrontendHttpBase.h
            ./StormSocketFrontendWebsocketBase.h
            ./StormSocketIOOperation.h
            ./StormSocketLog.h
            ./StormSocketRequest.h
            ./StormSocketServerFrontendHttp.h
            ./StormSocketServerFrontendWebsocket.h
            ./StormSocketServerTypes.h
            ./StormSocketServerWebsocket.h
            ./StormSocketServerWin.h
            ./StormUrlUtil.h
            ./StormWebsocketHeaderValues.h
            ./StormWebsocketMessageReader.h
            ./StormWebsocketMessageWriter.h
            )

if(MSVC)
  add_definitions(/DSECURITY_WIN32 /D_WIN32_WINNT=0x0601)
endif()

add_library(StormSocketCPP STATIC ${SRC_StormSocketCPP} ${HEADER_StormSocketCPP})
