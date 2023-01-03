#pragma once

#include "SculptorECS.h"


namespace spt::rsc
{

using RenderSceneEntity = ecs::Entity;

using RenderSceneRegistry = ecs::basic_registry<RenderSceneEntity>;

using RenderSceneEntityHandle = ecs::basic_handle<RenderSceneRegistry>;
using ConstRenderSceneEntityHandle = ecs::basic_handle<const RenderSceneRegistry>;

} // spt::rsc