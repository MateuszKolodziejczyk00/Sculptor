#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::rsc
{

class RenderScene;


class RENDER_SCENE_API PrimitiveSystem
{
public:

	explicit PrimitiveSystem(RenderScene& owningScene);

	virtual void Update() {};

protected:

	RenderScene& GetRenderScene() const;

private:

	RenderScene& m_owningScene;
};

} // spt::rsc