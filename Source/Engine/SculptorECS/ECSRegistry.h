#pragma once

#include "SculptorECSMacros.h"
#include "SculptorAliases.h"
#include "SculptorECS.h"


namespace spt::ecs
{

enum Entity : Uint32 {};

using Registry = ecs::basic_registry<Entity>;

using EntityHandle = ecs::basic_handle<Registry>;
using ConstEntityHandle = ecs::basic_handle<const Registry>;


SCULPTOR_ECS_API Registry& GetRegistry();

SCULPTOR_ECS_API EntityHandle CreateEntity();

} // spt::ecs
