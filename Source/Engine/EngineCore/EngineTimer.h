#pragma once

#include "EngineCoreMacros.h"
#include "Timer/TickingTimer.h"

namespace spt::engn
{

class ENGINE_CORE_API EngineTimer
{
public:

	EngineTimer();

	Real32 Tick();

	void Reset();

	void SetFixedDeltaTime(Real32 fixedDeltaTime);

	Real32 GetTime() const;

	Real32 GetDeltaTime() const;

	Real32 GetCurrentTimeSeconds() const;

private:

	Real32 m_currentTime;

	Real32					m_lastDeltaTime;
	std::optional<Real32>	m_fixedDeltaTime;
	lib::TickingTimer		m_tickingTimer;
};

} // spt::engn