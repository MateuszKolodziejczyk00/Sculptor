#include "TickingTimer.h"


namespace spt::lib
{

TickingTimer::TickingTimer()
{
	m_originTimestamp = m_lastTimestamp = GenerateNewTimestamp();
}

Real32 TickingTimer::Tick()
{
	const std::chrono::steady_clock::time_point newTimestamp = GenerateNewTimestamp();
	const Real32 deltaTime = std::chrono::duration<Real32, std::chrono::seconds::period>(newTimestamp - m_lastTimestamp).count();
	m_lastTimestamp = newTimestamp;

	return deltaTime;
}

Real32 TickingTimer::GetCurrentTimeSeconds() const
{
	return std::chrono::duration<Real32, std::chrono::seconds::period>(GenerateNewTimestamp() - m_originTimestamp).count();
}

std::chrono::steady_clock::time_point TickingTimer::GenerateNewTimestamp() const
{
	return std::chrono::high_resolution_clock::now();
}

}
