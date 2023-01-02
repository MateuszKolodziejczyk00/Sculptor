#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "Types/Buffer.h"
#include "SculptorECS.h"
#include "Utility/NamedType.h"

namespace spt::rsc
{

struct RenderInstanceData
{
	math::Transform3f transfrom;
};


struct EntityTransformHandle
{
	EntityTransformHandle() = default;

	explicit EntityTransformHandle(const rhi::RHISuballocation& inSuballocation)
		: transformSuballocation(inSuballocation)
	{ }

	rhi::RHISuballocation transformSuballocation;
};


using RenderSceneEntity = lib::NamedType<ecs::Entity, struct RenderSceneEntityParameter>;


class RENDER_SCENE_API RenderScene
{
public:

	RenderScene();

	ecs::Registry& GetRegistry();
	const ecs::Registry& GetRegistry() const;

	RenderSceneEntity CreateEntity(const RenderInstanceData& instanceData);
	void DestroyEntity(RenderSceneEntity entity);

	Uint64 GetTransformIdx(RenderSceneEntity entity) const;

private:

	ecs::Registry m_registry;

	lib::SharedPtr<rdr::Buffer> m_instanceTransforms;
};

} // spt::rsc
