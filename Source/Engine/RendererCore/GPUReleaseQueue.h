#pragma once

#include "SculptorCoreTypes.h"
#include "Delegates/MulticastDelegate.h"


namespace spt::rdr
{

class GPUReleaseQueue
{
	using Queue = lib::MulticastDelegate<void()>;

public:

	using ReleaseEntry = Queue::Delegate;

	void Enqueue(ReleaseEntry entry)
	{
		const lib::LockGuard lock(m_lock);

		m_queue.Add(std::move(entry));
	}

	Queue Flush()
	{
		const lib::LockGuard lock(m_lock);

		Queue copy = std::move(m_queue);

		SPT_CHECK(!m_queue.IsBound());

		return copy;
	}

	Bool IsEmpty() const
	{
		return !m_queue.IsBound();
	}

private:

	Queue     m_queue;
	lib::Lock m_lock;
};

} // spt::rdr