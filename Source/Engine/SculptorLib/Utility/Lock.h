#pragma once

#include <mutex>
#include <shared_mutex>


namespace spt::lib
{

using Lock = std::mutex;

template<typename TLockType>
using LockGuard = std::lock_guard<TLockType>;

template<typename ...Locks>
using MultiLockGuard = std::scoped_lock<Locks...>;

using ReadWriteLock = std::shared_mutex;

using ReadLockGuard = std::shared_lock<ReadWriteLock>;

using WriteLockGuard = std::lock_guard<ReadWriteLock>;

}