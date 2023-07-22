#pragma once

#include "RenderSceneMacros.h"
#include "SculptorAliases.h"
#include "SculptorECS.h"


namespace spt::rsc
{

enum RenderingDataEntity : Uint32 {};

using RenderingDataRegistry = ecs::basic_registry<RenderingDataEntity>;

using RenderingDataEntityHandle = ecs::basic_handle<RenderingDataRegistry>;
using ConstRenderingDataEntityHandle = ecs::basic_handle<const RenderingDataRegistry>;


class RENDER_SCENE_API RenderingData
{
public:

	static RenderingDataRegistry& Get();

	static RenderingDataEntityHandle CreateEntity();

private:

	static RenderingData& GetInstance();

	RenderingData() = default;

	RenderingDataRegistry m_registry;
};

} // spt::rsc
