#pragma once

#include "MathCore.h"
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
		SPT_PROFILER_FUNCTION();

		while (true)
		{
			if (TryLock())
			{
				break;
			}

			while (m_locked.load(std::memory_order_acquire) == false)
			{
				platf::Platform::SwitchToThread();
			}
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
