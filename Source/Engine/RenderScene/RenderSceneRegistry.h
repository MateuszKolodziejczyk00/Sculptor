#pragma once

#include "SculptorECS.h"
#include "ComponentsRegistry.h"


namespace spt::rsc
{

enum RenderSceneEntity : Uint32 {};

using RenderSceneRegistry = ecs::basic_registry<RenderSceneEntity>;

using RenderSceneEntityHandle = ecs::basic_handle<RenderSceneRegistry>;
using ConstRenderSceneEntityHandle = ecs::basic_handle<const RenderSceneRegistry>;

using NullEntity = ecs::GenericNullEntity;

constexpr NullEntity nullRenderSceneEntity{};

} // spt::rsc