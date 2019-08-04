

#include "StormSemaphore.h"


#ifdef _INCLUDEOS
#include <kernel/events.hpp>
#endif

#if defined(_WINDOWS) && defined(USE_NATIVE_SEMAPHORE)
#include <Windows.h>
#endif

namespace StormSockets
{
	void StormSemaphore::Init([[maybe_unused]] int max_count)
	{
#ifndef _INCLUDEOS

#if defined(_WINDOWS) && defined(USE_NATIVE_SEMAPHORE)
		m_Semaphore = CreateSemaphore(NULL, 0, max_count, NULL);
#endif

#endif
	}

#ifndef _INCLUDEOS
	bool StormSemaphore::WaitOne(int ms)
	{

#if defined(_WINDOWS) && defined(USE_NATIVE_SEMAPHORE)
		WaitForSingleObject(m_Semaphore, ms);
#else
        std::unique_lock<std::mutex> lock{ m_Mutex };
        auto finished = m_ConditionVariable.wait_for(lock, std::chrono::milliseconds(ms), [&] { return m_Count > 0; });

        if (finished)
        {
          --m_Count;
        }

        return finished;
#endif
	}

	void StormSemaphore::WaitOne()
	{
#if defined(_WINDOWS) && defined(USE_NATIVE_SEMAPHORE)
		WaitForSingleObject(m_Semaphore, INFINITE);
#else
        std::unique_lock<std::mutex> lock(m_Mutex);
        while (m_Count == 0)
        {
          m_ConditionVariable.wait(lock);
        }

        m_Count--;
#endif
	}
#endif

#ifdef _INCLUDEOS
    void StormSemaphore::SetDelegate(delegate<void()> del)
    {
        m_Delegate = del;
    }
#endif

	void StormSemaphore::Release([[maybe_unused]] int amount)
	{
#ifndef _INCLUDEOS
#if defined(_WINDOWS) && defined(USE_NATIVE_SEMAPHORE)
		ReleaseSemaphore(m_Semaphore, amount, NULL);
#else
        std::unique_lock<std::mutex> lock(m_Mutex);
        m_Count += amount;
        m_ConditionVariable.notify_one();
#endif
#else
        Events::get().defer(m_Delegate);
#endif
	}

}