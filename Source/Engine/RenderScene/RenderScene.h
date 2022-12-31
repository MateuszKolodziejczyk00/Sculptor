#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "SculptorECS.h"

namespace spt::rsc
{

class RENDER_SCENE_API RenderScene
{
public:

	RenderScene();

	void Initialize(entt::registry& registry);

	entt::registry& GetRegistry() const;

private:

	entt::registry* m_registry;
};

} // spt::rsc
