#pragma once

#include "SculptorECS.h"


namespace spt::rsc
{

enum RenderSceneEntity : Uint32 {};

class RenderSceneRegistry : public ecs::basic_registry<RenderSceneEntity>
{
protected:

	using Super = ecs::basic_registry<RenderSceneEntity>;

public:

	using Super::Super;
};

using RenderSceneEntityHandle = ecs::basic_handle<RenderSceneRegistry>;
using ConstRenderSceneEntityHandle = ecs::basic_handle<const RenderSceneRegistry>;

} // spt::rsc