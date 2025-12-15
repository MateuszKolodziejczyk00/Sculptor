#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::rsc
{

class RenderScene;
struct RenderSceneConstants;


class RENDER_SCENE_API RenderSceneSubsystem
{
public:

	explicit RenderSceneSubsystem(RenderScene& owningScene);
	virtual ~RenderSceneSubsystem() = default;

	virtual void Update() {};

	virtual void UpdateGPUSceneData(RenderSceneConstants& sceneData) {}

protected:

	RenderScene& GetOwningScene() const;

private:

	RenderScene& m_owningScene;
};

} // spt::rsc