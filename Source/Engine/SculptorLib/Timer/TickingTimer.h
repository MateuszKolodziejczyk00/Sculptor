#pragma once

#include "SculptorLibMacros.h"
#include <chrono>


namespace spt::lib
{

class SCULPTORLIB_API TickingTimer
{
public:

	TickingTimer();

	float Tick();

private:

	std::chrono::steady_clock::time_point GenerateNewTimestamp() const;

	std::chrono::steady_clock::time_point m_lastTimestamp;
};

}