#include "TickingTimer.h"


namespace spt::lib
{

TickingTimer::TickingTimer()
{
	m_lastTimestamp = GenerateNewTimestamp();
}

Real32 TickingTimer::Tick()
{
	const std::chrono::steady_clock::time_point newTimestamp = GenerateNewTimestamp();
	const Real32 deltaTime = std::chrono::duration<Real32, std::chrono::seconds::period>(m_lastTimestamp - newTimestamp).count();
	m_lastTimestamp = newTimestamp;

	return deltaTime;
}

std::chrono::steady_clock::time_point TickingTimer::GenerateNewTimestamp() const
{
	return std::chrono::high_resolution_clock::now();
}

}
