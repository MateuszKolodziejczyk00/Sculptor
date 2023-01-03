#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "Types/Buffer.h"
#include "RenderSceneRegistry.h"

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


class RENDER_SCENE_API RenderScene
{
public:

	RenderScene();

	ecs::Registry& GetRegistry();
	const ecs::Registry& GetRegistry() const;

	RenderSceneEntityHandle CreateEntity(const RenderInstanceData& instanceData);
	void DestroyEntity(RenderSceneEntityHandle entity);

	Uint64 GetTransformIdx(RenderSceneEntityHandle entity) const;

private:

	RenderSceneRegistry m_registry;

	lib::SharedPtr<rdr::Buffer> m_instanceTransforms;
};

} // spt::rsc
