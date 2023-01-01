#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "SculptorECS.h"
#include "RenderInstancesRegistry.h"

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

	RenderInstancesRegistry m_renderInstancesRegistry;
};

} // spt::rsc
