
#include "StormProfiling.h"


#ifdef _WINDOWS
#include <Windows.h>
#endif

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>


namespace StormSockets
{
	void StormProfilePrint(const char * fmt, ...)
	{
		return;
		
		char buffer[1024];
		va_list args;
		va_start(args, fmt);
		vsnprintf(buffer, sizeof(buffer), fmt, args);
		va_end(args);


#ifdef _WINDOWS
		long ms = GetTickCount();
#else

		long ms;
		struct timespec now;
		if (clock_gettime(CLOCK_MONOTONIC, &now))
		{
			ms = 0;
		}
		else
		{
			ms = now.tv_sec * 1000.0 + now.tv_nsec / 1000000.0;
		}
#endif

		printf("%lu: %s", ms, buffer);
	}

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