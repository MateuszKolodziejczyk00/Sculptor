#pragma once

#include "ProfilerCoreMacros.h"


namespace spt::prf
{

class ProfilerImpl
{
public:

	ProfilerImpl() = default;
	virtual ~ProfilerImpl() = default;

	virtual void BeginFrame() = 0;
	virtual void EndFrame() = 0;

	virtual void BeginEvent(const char* name) = 0;
	virtual void EndEvent() = 0;

	virtual void BeginThread(const char* name) = 0;
	virtual void EndThread() = 0;
};


class PROFILER_CORE_API ProfilerCore
{
public:

	static ProfilerCore& GetInstance();

	void Initialize();
	void Shutdown();

	void BeginFrame()
	{
		if (m_impl)
		{
			m_impl->BeginFrame();
		}
	}

	void EndFrame()
	{
		if (m_impl)
		{
			m_impl->EndFrame();
		}
	}

	void BeginEvent(const char* name)
	{
		if (m_impl)
		{
			m_impl->BeginEvent(name);
		}

	}

	void EndEvent()
	{
		if (m_impl)
		{
			m_impl->EndEvent();
		}
	}

	void BeginThread(const char* name)
	{
		if (m_impl)
		{
			m_impl->BeginThread(name);
		}
	}

	void EndThread()
	{
		if (m_impl)
		{
			m_impl->EndThread();
		}
	}

private:

	ProfilerCore();

	ProfilerImpl* m_impl;
};


struct PROFILER_CORE_API FrameScope
{
	FrameScope()
	{
		ProfilerCore::GetInstance().BeginFrame();
	}

	~FrameScope()
	{
		ProfilerCore::GetInstance().EndFrame();
	}
};


struct PROFILER_CORE_API EventScope
{
	EventScope(const char* name)
		: m_name(name)
	{
		ProfilerCore::GetInstance().BeginEvent(name);
	}

	~EventScope()
	{
		ProfilerCore::GetInstance().EndEvent();
	}

	const char* m_name;
};


struct PROFILER_CORE_API ThreadScope
{
	ThreadScope(const char* name)
		: m_name(name)
	{
		ProfilerCore::GetInstance().BeginThread(name);
	}

	~ThreadScope()
	{
		ProfilerCore::GetInstance().EndThread();
	}

	const char* m_name;
};

} // spt::prf


#if SPT_ENABLE_PROFILER

#define SPT_PROFILER_CORE_CONCAT_IMPL(begin, end) begin ## end
#define SPT_PROFILER_CORE_CONCAT(begin, end) SPT_PROFILER_CORE_CONCAT_IMPL(begin, end)

#define SPT_PROFILER_CORE_SCOPE_NAME(name) SPT_PROFILER_CORE_CONCAT(name, __LINE__)

#define SPT_PROFILER_FRAME(Name)		spt::prf::FrameScope SPT_PROFILER_CORE_SCOPE_NAME(frameScope)
#define SPT_PROFILER_FUNCTION()			spt::prf::EventScope SPT_PROFILER_CORE_SCOPE_NAME(eventScope)(__FUNCTION__)
#define SPT_PROFILER_SCOPE(Name)		spt::prf::EventScope SPT_PROFILER_CORE_SCOPE_NAME(eventScope)(Name)
#define SPT_PROFILER_THREAD(Name)		spt::prf::ThreadScope SPT_PROFILER_CORE_SCOPE_NAME(threadScope)(Name)

#else // SPT_ENABLE_PROFILER

#define SPT_PROFILER_FRAME(Name)
#define SPT_PROFILER_FUNCTION()
#define SPT_PROFILER_SCOPE(Name)	
#define SPT_PROFILER_THREAD(Name)

#endif // SPT_ENABLE_PROFILER