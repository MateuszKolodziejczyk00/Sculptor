#include "EngineTimer.h"

namespace spt::engn
{

EngineTimer::EngineTimer()
	: m_currentTime(0.f)
	, m_lastDeltaTime(0.f)
{ }

Real32 EngineTimer::Tick()
{
	const Real32 measuredDeltaTime = m_tickingTimer.Tick();
	const Real32 deltaTime = m_fixedDeltaTime.value_or(measuredDeltaTime);
	m_lastDeltaTime = deltaTime;
	m_currentTime += deltaTime;

	return deltaTime;
}

void EngineTimer::Reset()
{
	m_currentTime   = 0.f;
	m_lastDeltaTime = 0.f;
	m_tickingTimer.Tick();
}

void EngineTimer::SetFixedDeltaTime(Real32 fixedDeltaTime)
{
	SPT_CHECK(fixedDeltaTime > 0.f);
	m_fixedDeltaTime = fixedDeltaTime;
}

Real32 EngineTimer::GetTime() const
{
	return m_currentTime;
}

Real32 EngineTimer::GetDeltaTime() const
{
	return m_lastDeltaTime;
}

Real32 EngineTimer::GetCurrentTimeSeconds() const
{
	return m_tickingTimer.GetCurrentTimeSeconds();
}

} // spt::engn
