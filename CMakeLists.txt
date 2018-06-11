
cmake_minimum_required(VERSION 3.5)

include_directories(.. ../External)
add_definitions(-DUSE_MBED)

if(UNIX)
  add_definitions(-D_LINUX)
endif()

if(MSVC)
  add_definitions(-D_WIN32_WINNT=0x0A00)
  add_definitions(-DSECURITY_WIN32)
endif()

set(CMAKE_CXX_STANDARD 14)

add_library(StormSockets STATIC
	StormFixedBlockAllocator.cpp
	StormHttpBodyReader.cpp
	StormHttpHeaderValues.cpp
	StormHttpRequestReader.cpp
	StormHttpRequestWriter.cpp
	StormHttpResponseReader.cpp
	StormHttpResponseWriter.cpp
	StormMessageHeaderReader.cpp
	StormMessageHeaderValues.cpp
	StormMessageReaderCursor.cpp
	StormMessageReaderUtil.cpp
	StormMessageWriter.cpp
	StormProfiling.cpp
	StormSemaphore.cpp
	StormSha1.cpp
	StormSocketBackend.cpp
	StormSocketBuffer.cpp
	StormSocketClientFrontendHttp.cpp
	StormSocketClientFrontendWebsocket.cpp
	StormSocketConnectionId.cpp
	StormSocketFrontendBase.cpp
	StormSocketFrontendHttpBase.cpp
	StormSocketFrontendWebsocketBase.cpp
	StormSocketLog.cpp
	StormSocketServerFrontendHttp.cpp
	StormSocketServerFrontendWebsocket.cpp
	StormSocketServerWebsocket.cpp
	StormUrlUtil.cpp
	StormWebsocketHeaderValues.cpp
	StormWebsocketMessageReader.cpp
	StormWebsocketMessageWriter.cpp)


