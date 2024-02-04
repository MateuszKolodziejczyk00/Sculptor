#pragma once

#include "SculptorLibMacros.h"
#include "SculptorCoreTypes.h"
#include <chrono>


namespace spt::lib
{

class SCULPTOR_LIB_API TickingTimer
{
public:

	TickingTimer();

	Real32 Tick();

	Real32 GetCurrentTimeSeconds() const;

private:

	std::chrono::steady_clock::time_point GenerateNewTimestamp() const;

	std::chrono::steady_clock::time_point m_originTimestamp;
	std::chrono::steady_clock::time_point m_lastTimestamp;
};

} // std::lib