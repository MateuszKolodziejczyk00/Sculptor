#pragma once

#include <mutex>
#include <shared_mutex>


namespace spt::lib
{

using Lock = std::mutex;
using RecursiveLock = std::recursive_mutex;

template<typename TLockType>
using LockGuard = std::lock_guard<TLockType>;

template<typename TLockType>
using UnlockableLockGuard = std::unique_lock<TLockType>;

template<typename ...Locks>
using MultiLockGuard = std::scoped_lock<Locks...>;

using SharedLock = std::shared_mutex;
using ReadWriteLock = SharedLock;

using SharedLockGuard = std::shared_lock<SharedLock>;
using ReadLockGuard = SharedLockGuard;

using UniqueLockGuard = std::lock_guard<SharedLock>;
using WriteLockGuard = UniqueLockGuard;

}