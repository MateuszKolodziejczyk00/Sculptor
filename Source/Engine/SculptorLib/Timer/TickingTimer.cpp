#include "TickingTimer.h"


namespace spt::lib
{

TickingTimer::TickingTimer()
{
	m_lastTimestamp = GenerateNewTimestamp();
}

float TickingTimer::Tick()
{
	const std::chrono::steady_clock::time_point newTimestamp = GenerateNewTimestamp();
	const float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(m_lastTimestamp - newTimestamp).count();
	m_lastTimestamp = newTimestamp;

	return deltaTime;
}

std::chrono::steady_clock::time_point TickingTimer::GenerateNewTimestamp() const
{
	return std::chrono::high_resolution_clock::now();
}

}
