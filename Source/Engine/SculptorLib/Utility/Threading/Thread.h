#pragma once

#include <thread>
#include "SculptorAliases.h"


namespace spt::lib
{


using ThreadID = std::thread::id;


class CurrentThread
{
public:

	static ThreadID CurrentID()
	{
		return std::this_thread::get_id();
	}

	static void SleepFor(Uint64 milliseconds)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
	}

	static void Yield()
	{
		std::this_thread::yield();
	}
};


class Thread
{
public:

	Thread() = default;

	template<typename Function, typename... Args>
	Thread(Function&& function, Args&&... args)
		: m_thread(std::forward<Function>(function), std::forward<Args>(args)...)
	{
	}

	~Thread()
	{
		if (m_thread.joinable())
		{
			m_thread.join();
		}
	}

	Thread(const Thread&) = delete;
	Thread& operator=(const Thread&) = delete;

	Thread(Thread&& other) noexcept
		: m_thread(std::move(other.m_thread))
	{
	}

	Thread& operator=(Thread&& other) noexcept
	{
		m_thread = std::move(other.m_thread);
		return *this;
	}
	
	void Join()
	{
		m_thread.join();
	}

	void Detach()
	{
		m_thread.detach();
	}

	ThreadID GetID() const
	{
		return m_thread.get_id();
	}

	Bool IsValid() const
	{
		return GetID() != ThreadID{};
	}

private:

	std::thread m_thread;
};

} // spt::lib