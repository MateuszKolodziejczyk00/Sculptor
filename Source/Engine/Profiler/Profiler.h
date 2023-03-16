#pragma once

#include "ProfilerMacros.h"
#include "SculptorCoreTypes.h"
#include "Delegates/MulticastDelegate.h"


namespace spt::prf
{

class PROFILER_API Profiler
{
public:

	static Profiler& Get();

	void Initialize();

	// Captures ===================================================

	void StartCapture();
	void StopAndSaveCapture();

	Bool StartedCapture() const;

	// Frame Time =================================================

	Real32 GetFrameTime(SizeType idx) const;
	SizeType GetCollectedFrameTimesNum() const;
	Real32 GetAverageFrameTime() const;

private:

	Profiler();

	void OnBeginNewFrame();

	Bool m_startedCapture;

	lib::StaticArray<Real32, 100>	m_recentFrameTimes;
	SizeType						m_recentFrameTimesNum;
	SizeType						m_oldestFrameTimeIdx;
	Real32							m_recentFrameTimesSum;

	lib::DelegateHandle				m_captureDelegateHandle;
};

} // spt::prf