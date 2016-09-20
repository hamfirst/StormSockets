

#include "StormSemaphore.h"

#if defined(_WINDOWS) && defined(USE_NATIVE_SEMAPHORE)
#include <Windows.h>
#endif

namespace StormSockets
{
	void StormSemaphore::Init(int max_count)
	{
#if defined(_WINDOWS) && defined(USE_NATIVE_SEMAPHORE)
		m_Semaphore = CreateSemaphore(NULL, 0, max_count, NULL);
#endif
	}

	void StormSemaphore::WaitOne(int ms)
	{
#if defined(_WINDOWS) && defined(USE_NATIVE_SEMAPHORE)
		WaitForSingleObject(m_Semaphore, ms);
#else
    std::unique_lock<std::mutex> lck(m_Mutex);
    while (m_Count == 0)
    {
      m_ConditionVariable.wait_for(lck, std::chrono::milliseconds(ms));
    }

    m_Count--;
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

	void StormSemaphore::Release(int amount)
	{
#if defined(_WINDOWS) && defined(USE_NATIVE_SEMAPHORE)
		ReleaseSemaphore(m_Semaphore, amount, NULL);
#else
    std::unique_lock<std::mutex> lock(m_Mutex);
    m_Count += amount;
    m_ConditionVariable.notify_one();
#endif
	}
}