
#include "StormProfiling.h"


#ifdef _WINDOWS
#include <Windows.h>
#endif

#include <stdio.h>


namespace StormSockets
{
	std::atomic_uint_fast64_t s_Timers[ProfilerCategory::kCount];

	uint64_t Profiling::StartProfiler()
	{
		uint64_t val = 0;

#ifdef _WINDOWS
		_LARGE_INTEGER li;
		QueryPerformanceCounter(&li);
		val = li.QuadPart;
#endif
		return val;
	}

	void Profiling::EndProfiler(uint64_t start_val, ProfilerCategory::Index category)
	{
		uint64_t now = 0;
#ifdef _WINDOWS
		_LARGE_INTEGER li;
		QueryPerformanceCounter(&li);
		now = li.QuadPart;
#endif
		uint64_t diff = now - start_val;
		s_Timers[category].fetch_add(diff);
	}

	void Profiling::Print()
	{
#ifdef _WINDOWS
		uint64_t freq = 0;
		_LARGE_INTEGER li;
		QueryPerformanceFrequency(&li);
		freq = li.QuadPart;
		freq /= 1000000;

		for (int index = 0; index < ProfilerCategory::kCount; index++)
		{
			printf("%d - %lld\n", index, s_Timers[index].load() / freq);
		}
#endif
	}
}