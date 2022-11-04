#pragma once

#include "ProfilerMacros.h"
#include "UILayers/UILayer.h"


namespace spt::prf
{

class PROFILER_API ProfilerUILayer : public scui::UILayer
{
protected:

	using Super = scui::UILayer;

public:

	explicit ProfilerUILayer(const scui::LayerDefinition& definition);

protected:

	//~ Begin  UILayer overrides
	virtual void DrawUI() override;
	//~ End  UILayer overrides

private:

	lib::StaticArray<float, 64>	m_lastFrameTimes;
	SizeType					m_oldestFrameTimeIdx;
};

} // spt::prf
