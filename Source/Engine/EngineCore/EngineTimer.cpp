#include "EngineTimer.h"

namespace spt::engn
{

EngineTimer::EngineTimer()
	: m_currentTime(0.f)
	, m_lastDeltaTime(0.f)
{ }

Real32 EngineTimer::Tick()
{
	Real32 deltaTime = m_tickingTimer.Tick();
	m_lastDeltaTime = deltaTime;
	m_currentTime += deltaTime;

	return deltaTime;
}

Real32 EngineTimer::GetTime() const
{
	return m_currentTime;
}

Real32 EngineTimer::GetDeltaTime() const
{
	return m_lastDeltaTime;
}

} // spt::engn
