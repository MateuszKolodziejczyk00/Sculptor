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

	Real32 GetTime() const;

	Real32 GetDeltaTime() const;

private:

	Real32 m_currentTime;

	Real32				m_lastDeltaTime;
	lib::TickingTimer	m_tickingTimer;
};

} // spt::engn