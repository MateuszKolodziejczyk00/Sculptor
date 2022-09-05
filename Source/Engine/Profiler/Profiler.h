#pragma once

#include "ProfilerMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::prf
{

class PROFILER_API Profiler
{
public:

	static void StartCapture();
	static void StopCapture();

	static Bool StartedCapture();

	static Bool SaveCapture();
};

} // spt::prf