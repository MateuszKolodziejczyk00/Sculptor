#pragma once

#include "ProfilerMacros.h"
#include "UILayers/UIView.h"


namespace spt::rdr
{
struct GPUStatisticsScopeResult;
} // spt::rdr


namespace spt::prf
{

struct GPUProfilerStatistics;


class PROFILER_API ProfilerUIView : public scui::UIView
{
protected:

	using Super = scui::UIView;

public:

	explicit ProfilerUIView(const scui::ViewDefinition& definition);

protected:

	//~ Begin  UIView overrides
	virtual void BuildDefaultLayout(ImGuiID dockspaceID) override;
	virtual void DrawUI() override;
	//~ End  UIView overrides

private:

	void DrawGPUProfilerUI();

	void DrawGPUScopeStatistics(const GPUProfilerStatistics& profilerStats);
	void DrawGPUScopeStatistics(const rdr::GPUStatisticsScopeResult& scopeStats, Real32 frameDuration);

	lib::StaticArray<float, 64>	m_lastFrameTimes;
	SizeType					m_oldestFrameTimeIdx;

	lib::HashedString m_profilerPanelName;
};

} // spt::prf
