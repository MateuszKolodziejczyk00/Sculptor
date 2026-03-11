#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "RenderSceneRegistry.h"
#include "RenderSceneTypes.h"


namespace spt::engn
{
class FrameContext;
} // spt::engn


namespace spt::rsc
{

struct EntityGPUDataHandle
{
	EntityGPUDataHandle() = default;

	explicit EntityGPUDataHandle(const rhi::RHIVirtualAllocation& inSuballocation)
		: transformSuballocation(inSuballocation)
	{ }

	RenderEntityGPUPtr GetGPUDataPtr() const
	{
		return RenderEntityGPUPtr(static_cast<Uint32>(transformSuballocation.GetOffset()));
	}

	rhi::RHIVirtualAllocation transformSuballocation;
};
SPT_REGISTER_COMPONENT_TYPE(EntityGPUDataHandle, RenderSceneRegistry);


class RENDER_SCENE_API RenderScene
{
public:

	RenderScene();

	RenderSceneRegistry& GetRegistry();
	const RenderSceneRegistry& GetRegistry() const;

	void BeginFrame(const engn::FrameContext& frame);
	void EndFrame();

	const engn::FrameContext& GetCurrentFrameRef() const;

	// Entities =============================================================

	RenderSceneEntityHandle CreateEntity();
	RenderSceneEntityHandle CreateEntity(const RenderInstanceData& instanceData);
	void DestroyEntity(RenderSceneEntityHandle entity);

	// Rendering ============================================================

	const lib::SharedRef<rdr::Buffer>& GetRenderEntitiesBuffer() const;

private:

	lib::SharedRef<rdr::Buffer> CreateInstancesBuffer() const;

	RenderSceneRegistry m_registry;

	lib::SharedRef<rdr::Buffer> m_renderEntitiesBuffer;

	lib::SharedPtr<const engn::FrameContext> m_currentFrame;
};

} // spt::rsc
