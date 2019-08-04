
#pragma once

#if defined(_WINDOWS) && defined(USE_NATIVE_SEMAPHORE)
typedef void * Semaphore_t;
#else

#ifndef _INCLUDEOS

#include <mutex>
#include <condition_variable>

#else

#include <delegate>

#endif

#endif

namespace StormSockets
{
	class StormSemaphore
	{
#ifndef _INCLUDEOS

#if defined(_WINDOWS) && defined(USE_NATIVE_SEMAPHORE)
    Semaphore_t m_Semaphore;
#else
    std::mutex m_Mutex;
    std::condition_variable m_ConditionVariable;
    volatile int m_Count = 0;
#endif

#else
		delegate<void()> m_Delegate;
#endif

	public:
		void Init(int max_count);

#ifndef _INCLUDEOS
		bool WaitOne(int ms);
		void WaitOne();
#else
		void SetDelegate(delegate<void()> del);
#endif

		void Release(int amount = 1);
	};
}
