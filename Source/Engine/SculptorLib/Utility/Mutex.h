#pragma once

#include <mutex>
#include <shared_mutex>


namespace spt::lib
{

using Lock = std::mutex;

template<typename TLockType>
using LockGuard = std::lock_guard<TLockType>;

using ReadWriteLock = std::shared_mutex;

using ReadLockGuard = std::shared_lock<ReadWriteLock>;

using WriteLockGuard = std::unique_lock<ReadWriteLock>;

}