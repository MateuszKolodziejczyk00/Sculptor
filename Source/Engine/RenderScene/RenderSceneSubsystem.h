#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::rsc
{

class RenderScene;


class RENDER_SCENE_API RenderSceneSubsystem
{
public:

	explicit RenderSceneSubsystem(RenderScene& owningScene);
	virtual ~RenderSceneSubsystem() = default;

	virtual void Update() {};

protected:

	RenderScene& GetOwningScene() const;

private:

	RenderScene& m_owningScene;
};

} // spt::rsc