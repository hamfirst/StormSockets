
#pragma once

#include <stdint.h>
#include <atomic>

namespace StormSockets
{

	void StormProfilePrint(const char * fmt, ...);

	namespace ProfilerCategory
	{
		enum Index
		{
			kWriteByte,
			kReadByte,
			kSend,
			kEnqueue,
			kAllocArraySlot,
			kReleaseArraySlot,
			kPacketCopy,
			kCreatePacket,
			kFinalizePacket,
			kPrepRecv,
			kProcData,
      kProcHttp,
      kProcHeaders,
      kProcChunk,
      kProcChunkSize,
      kRepost,
      kSSLDecrypt,
      kSSLEncrypt,
      kFullRequest,
			kCount,
		};
	}

	class Profiling
	{

	public:
		static uint64_t StartProfiler();
		static void EndProfiler(uint64_t start_val, ProfilerCategory::Index category);

		static void Print();
	};

  class ProfileScope
  {
    uint64_t m_Prof;
    ProfilerCategory::Index m_Category;
  public:
    ProfileScope(ProfilerCategory::Index category) { m_Prof = Profiling::StartProfiler(); m_Category = category; }
    ~ProfileScope() { Profiling::EndProfiler(m_Prof, m_Category); }
  };
}
