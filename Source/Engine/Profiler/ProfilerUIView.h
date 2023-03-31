#pragma once

#include "ProfilerMacros.h"
#include "UILayers/UIView.h"


namespace spt::rdr
{
struct GPUStatisticsScopeResult;
} // spt::rdr


namespace spt::prf
{

class PROFILER_API ProfilerUIView : public scui::UIView
{
protected:

	using Super = scui::UIView;

public:

	explicit ProfilerUIView(const scui::ViewDefinition& definition);

protected:

	//~ Begin  UIView overrides
	virtual void DrawUI() override;
	//~ End  UIView overrides

private:

	void DrawGPUProfilerUI();

	void DrawGPUScopeStatistics(const rdr::GPUStatisticsScopeResult& scopeStats, Real32 frameDuration);

	lib::StaticArray<float, 64>	m_lastFrameTimes;
	SizeType					m_oldestFrameTimeIdx;
};

} // spt::prf
