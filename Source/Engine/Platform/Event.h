#pragma once

#include "PlatformMacros.h"
#include "SculptorAliases.h"


namespace spt::platf
{

class PLATFORM_API Event
{
public:

	explicit Event(const wchar_t* name, Bool manualReset, Bool initialState = false);
	~Event();

	void Trigger();
	void Reset();

	void Wait();

private:

	IntPtr m_handle;
};

} // spt::platf