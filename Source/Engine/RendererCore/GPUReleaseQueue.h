#pragma once

#include "SculptorCoreTypes.h"
#include "Delegates/MulticastDelegate.h"


namespace spt::rdr
{

class GPUReleaseQueue
{
public:

	using ReadyQueue = lib::MulticastDelegate<void()>;

	using ReleaseEntry = ReadyQueue::Delegate;

	void Enqueue(ReleaseEntry entry)
	{
		const lib::LockGuard lock(m_lock);

		m_queue.Add(std::move(entry));
	}

	ReadyQueue Flush()
	{
		const lib::LockGuard lock(m_lock);

		ReadyQueue copy = std::move(m_queue);

		SPT_CHECK(!m_queue.IsBound());

		return copy;
	}

	Bool IsEmpty() const
	{
		return !m_queue.IsBound();
	}

private:

	ReadyQueue m_queue;
	lib::Lock  m_lock;
};

} // spt::rdr
