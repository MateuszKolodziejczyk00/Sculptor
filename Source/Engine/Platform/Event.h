#pragma once

#include "PlatformMacros.h"
#include "SculptorAliases.h"


namespace spt::platf
{

class PLATFORM_API Event
{
public:

	explicit Event(const wchar_t* name, Bool manualReset, Bool initialState = false);
	explicit Event(Bool manualReset, Bool initialState = false);
	~Event();

	Event(Event&& rhs)				= delete;
	Event& operator=(Event&& rhs)	= delete;

	Event(const Event& rhs)				= delete;
	Event& operator=(const Event& rhs)	= delete;

	void Trigger();
	void Reset();

	Bool IsTriggered() const;

	void Wait();

private:

	IntPtr m_handle;
};

} // spt::platf