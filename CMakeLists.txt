cmake_minimum_required(VERSION 3.0)

include_directories(. ../. .././External)
set(CMAKE_CXX_STANDARD 14)

set(GENERIC_SRC_StormSocketCPP 
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
            #CPP_PLACEHOLDER
            )
set(GENERIC_HEADER_StormSocketCPP 
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
            #HEADER_PLACEHOLDER
            )
set(GENERIC_REFL_StormSocketCPP 
            #REFL_PLACEHOLDER
            )

if (MSVC)
  add_definitions(-D_WINDOWS)
  include_directories(.././External/Windows)
  set(PLATFORM_SRC_StormSocketCPP 
            #CPP_WINDOWS_PLACEHOLDER
            )
endif()
if (WEB)
  add_definitions(-D_WEB)
  include_directories(.././External/Web)
  set(PLATFORM_SRC_StormSocketCPP 
            #CPP_WEB_PLACEHOLDER
            )
endif()
if (IOS)
  add_definitions(-D_IOS)
  include_directories(.././External/IOS)
  set(PLATFORM_SRC_StormSocketCPP 
            #CPP_IOS_PLACEHOLDER
            )
endif()
if (APPLE && NOT IOS)
  add_definitions(-D_MACOS)
  include_directories(.././External/MacOS)
  include_directories(/usr/include/freetype2)
  set(PLATFORM_SRC_StormSocketCPP 
            #CPP_MACOS_PLACEHOLDER
            )
endif()
if (ANDROID)
  add_definitions(-D_ANDROID)
  include_directories(.././External/Android)
  set(PLATFORM_SRC_StormSocketCPP 
            #CPP_ANDROID_PLACEHOLDER
            )
endif()
if (UNIX AND NOT APPLE)
  add_definitions(-D_LINUX)
  include_directories(.././External/Linux)
  include_directories(/usr/include/freetype2)
  set(PLATFORM_SRC_StormSocketCPP 
            #CPP_LINUX_PLACEHOLDER
            )
endif()

foreach(REFL_FILE StormSocketCPP ${GENERIC_REFL_StormSocketCPP} ${PLATFORM_REFL_StormSocketCPP})
  string(REPLACE ".refl.h" ".refl.meta.h" META_FILE ${REFIL_FILE})
  add_custom_command(OUTPUT ${META_FILE}
                     COMMAND StormRefl -- -DSTORM_REFL_PARSE -D_CRT_SECURE_NO_WARNINGS -x c++ -Wno-pragma-once-outside-header -I. -I../. -I.././External
                     MAIN_DEPENDENCY ${REFL_FILE}
                     IMPLICIT_DEPENDS CXX ${REFL_FILE})
endforeach()

add_library(StormSocketCPP ${GENERIC_SRC_StormSocketCPP} ${PLATFORM_SRC_StormSocketCPP}
            StormSocketCPP ${GENERIC_HEADER_StormSocketCPP} ${PLATFORM_HEADER_StormSocketCPP})