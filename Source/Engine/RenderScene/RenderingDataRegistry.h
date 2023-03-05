#pragma once

#include "RenderSceneMacros.h"
#include "SculptorECS.h"


namespace spt::rsc
{

using RenderingDataEntity = ecs::Entity;

class RenderingDataRegistry : public ecs::basic_registry<RenderingDataEntity>
{
protected:

	using Super = ecs::basic_registry<RenderingDataEntity>;

public:

	using Super::Super;
};

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
