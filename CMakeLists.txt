
cmake_minimum_required(VERSION 3.5)

include_directories(.. ../External)
add_definitions(-DUSE_MBED -D_LINUX)

set(CMAKE_CXX_STANDARD 14)


add_library(StormSockets STATIC
	../External/asio/asio.cpp
	../External/mbedtls/aes.c
	../External/mbedtls/aesni.c
	../External/mbedtls/arc4.c
	../External/mbedtls/asn1parse.c
	../External/mbedtls/asn1write.c
	../External/mbedtls/base64.c
	../External/mbedtls/bignum.c
	../External/mbedtls/blowfish.c
	../External/mbedtls/camellia.c
	../External/mbedtls/ccm.c
	../External/mbedtls/certs.c
	../External/mbedtls/cipher.c
	../External/mbedtls/cipher_wrap.c
	../External/mbedtls/ctr_drbg.c
	../External/mbedtls/debug.c
	../External/mbedtls/des.c
	../External/mbedtls/dhm.c
	../External/mbedtls/ecdh.c
	../External/mbedtls/ecdsa.c
	../External/mbedtls/ecjpake.c
	../External/mbedtls/ecp.c
	../External/mbedtls/ecp_curves.c
	../External/mbedtls/entropy.c
	../External/mbedtls/entropy_poll.c
	../External/mbedtls/error.c
	../External/mbedtls/gcm.c
	../External/mbedtls/havege.c
	../External/mbedtls/hmac_drbg.c
	../External/mbedtls/md.c
	../External/mbedtls/md2.c
	../External/mbedtls/md4.c
	../External/mbedtls/md5.c
	../External/mbedtls/md_wrap.c
	../External/mbedtls/memory_buffer_alloc.c
	../External/mbedtls/net.c
	../External/mbedtls/oid.c
	../External/mbedtls/padlock.c
	../External/mbedtls/pem.c
	../External/mbedtls/pk.c
	../External/mbedtls/pkcs11.c
	../External/mbedtls/pkcs12.c
	../External/mbedtls/pkcs5.c
	../External/mbedtls/pkparse.c
	../External/mbedtls/pkwrite.c
	../External/mbedtls/pk_wrap.c
	../External/mbedtls/platform.c
	../External/mbedtls/ripemd160.c
	../External/mbedtls/rsa.c
	../External/mbedtls/sha1.c
	../External/mbedtls/sha256.c
	../External/mbedtls/sha512.c
	../External/mbedtls/ssl_cache.c
	../External/mbedtls/ssl_ciphersuites.c
	../External/mbedtls/ssl_cli.c
	../External/mbedtls/ssl_cookie.c
	../External/mbedtls/ssl_srv.c
	../External/mbedtls/ssl_ticket.c
	../External/mbedtls/ssl_tls.c
	../External/mbedtls/threading.c
	../External/mbedtls/timing.c
	../External/mbedtls/version.c
	../External/mbedtls/version_features.c
	../External/mbedtls/x509.c
	../External/mbedtls/x509write_crt.c
	../External/mbedtls/x509write_csr.c
	../External/mbedtls/x509_create.c
	../External/mbedtls/x509_crl.c
	../External/mbedtls/x509_crt.c
	../External/mbedtls/x509_csr.c
	../External/mbedtls/xtea.c
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
	StormSocketServerFrontendHttp.cpp
	StormSocketServerFrontendWebsocket.cpp
	StormSocketServerWebsocket.cpp
	StormUrlUtil.cpp
	StormWebsocketHeaderValues.cpp
	StormWebsocketMessageReader.cpp
	StormWebsocketMessageWriter.cpp)


