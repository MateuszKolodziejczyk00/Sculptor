#pragma once

#include "SculptorLibMacros.h"
#include "SculptorCoreTypes.h"
#include <chrono>


namespace spt::lib
{

class SCULPTORLIB_API TickingTimer
{
public:

	TickingTimer();

	Real32 Tick();

private:

	std::chrono::steady_clock::time_point GenerateNewTimestamp() const;

	std::chrono::steady_clock::time_point m_lastTimestamp;
};

}