
#pragma once

#ifndef _INCLUDEOS
#include <mutex>

namespace StormSockets
{
  using StormMutex = std::mutex;

  template <typename MutexType>
  using StormUniqueLock = std::unique_lock<MutexType>;


  template <typename MutexType>
  using StormLockGuard = std::lock_guard<MutexType>;
}
#else

namespace StormSockets
{
  struct StormMutex {};

  template <typename MutexType>
  struct StormUniqueLock
  {
    StormUniqueLock(MutexType &)
    {

    }

    template <typename DeferType>
    StormUniqueLock(MutexType &, DeferType &&)
    {

    }

    void lock()
    {
      locked = true;
    }

    void unlock()
    {
      locked = false;
    }

    bool owns_lock()
    {
      return locked;
    }

    bool locked = false;
  };

  template <typename MutexType>
  struct StormLockGuard
  {
    StormLockGuard(MutexType &)
    {

    }
  };
}

#endif


