#pragma once

#include "EditorCommonMacros.h"
#include "EngineFrame.h"


namespace spt::ed
{

struct TerrainEditorFrameState;
struct CloudscapeEditorFrameState;


class EDITOR_COMMON_API EditorFrameContext : public engn::FrameContext
{
protected:

	using Super = engn::FrameContext;

public:

	EditorFrameContext();
	~EditorFrameContext();

	TerrainEditorFrameState*                         terrainEditorState = nullptr;
	lib::RawCallable<void(TerrainEditorFrameState*)> terrainEditorStateDeleter;

	CloudscapeEditorFrameState*                         cloudscapeEditorState = nullptr;
	lib::RawCallable<void(CloudscapeEditorFrameState*)> cloudscapeEditorStateDeleter;

protected:

	// Begin engn::FrameContext overrides
	virtual void DoStagesTransition(engn::EFrameStage::Type prevStage, engn::EFrameStage::Type nextStage) override;
	// End engn::FrameContext overrides
};

} // spt::ed
