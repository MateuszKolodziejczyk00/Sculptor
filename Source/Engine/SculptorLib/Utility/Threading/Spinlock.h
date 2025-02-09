#pragma once

#include "SculptorAliases.h"
#include "Platform.h"
#include "ProfilerCore.h"
#include "Utility/UtilityMacros.h"


namespace spt::lib
{

class Spinlock
{
public:

	Spinlock()
		: m_locked(false)
	{ }

	// Helpers to allow use with std::lock_guard
	void lock()
	{
		Lock();
	}

	void unlock()
	{
		Unlock();
	}

	void Lock()
	{
		bool wasLocked = false;
		while (m_locked.load() || !m_locked.compare_exchange_weak(INOUT wasLocked, true))
		{
			_mm_pause();
			wasLocked = false;
		}
	}

	void Unlock()
	{
		m_locked.store(false, std::memory_order_release);
	}

	SPT_NODISCARD Bool TryLock()
	{
		const Bool wasLocked = m_locked.exchange(true, std::memory_order_acquire);
		return !wasLocked;
	}

private:

	std::atomic<Bool> m_locked;
};

} // spt::lib
