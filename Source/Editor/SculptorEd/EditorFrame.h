#pragma once

#include "EngineFrame.h"


namespace spt::ed
{

class EditorFrameContext : public engn::FrameContext
{
protected:

	using Super = engn::FrameContext;

public:

	EditorFrameContext();

protected:

	// Begin engn::FrameContext overrides
	virtual void DoStagesTransition(engn::EFrameStage::Type prevStage, engn::EFrameStage::Type nextStage) override;
	// End engn::FrameContext overrides
};

} // spt::ed