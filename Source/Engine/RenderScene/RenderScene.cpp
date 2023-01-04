#include "RenderScene.h"
#include "ResourcesManager.h"
#include "BufferUtilities.h"

namespace spt::rsc
{

RenderScene::RenderScene()
{
	rhi::RHIAllocationInfo transformsAllocationInfo;
	transformsAllocationInfo.memoryUsage = rhi::EMemoryUsage::GPUOnly;
	transformsAllocationInfo.allocationFlags = rhi::EAllocationFlags::CreateDedicatedAllocation;

	rhi::BufferDefinition transformsBufferDef;
	transformsBufferDef.size = 1024 * sizeof(math::Transform3f);
	transformsBufferDef.usage = rhi::EBufferUsage::Storage;
	transformsBufferDef.flags = rhi::EBufferFlags::WithVirtualSuballocations;

	m_instanceTransforms = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("InstanceTransformsBuffer"), transformsBufferDef, transformsAllocationInfo);
}

ecs::Registry& RenderScene::GetRegistry()
{
	return m_registry;
}

const ecs::Registry& RenderScene::GetRegistry() const
{
	return m_registry;
}

RenderSceneEntityHandle RenderScene::CreateEntity(const RenderInstanceData& instanceData)
{
	SPT_PROFILER_FUNCTION();

	const RenderSceneEntity entityID = m_registry.create();
	const RenderSceneEntityHandle entity(m_registry, entityID);

	const rhi::SuballocationDefinition suballocationDef(sizeof(math::Transform3f), sizeof(math::Transform3f), rhi::EBufferSuballocationFlags::PreferFasterAllocation);
	const rhi::RHISuballocation entityTransformSuballocation = m_instanceTransforms->GetRHI().CreateSuballocation(suballocationDef);

	entity.emplace<EntityTransformHandle>(entityTransformSuballocation);

	const Byte* transformData = reinterpret_cast<const Byte*>(instanceData.transfrom.data());
	gfx::UploadDataToBuffer(lib::Ref(m_instanceTransforms), entityTransformSuballocation.GetOffset(), transformData, sizeof(math::Transform3f));

	return entity;
}

void RenderScene::DestroyEntity(RenderSceneEntityHandle entity)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK_NO_ENTRY_MSG("TODO");
}

Uint64 RenderScene::GetTransformIdx(RenderSceneEntityHandle entity) const
{
	SPT_PROFILER_FUNCTION();

	const EntityTransformHandle& transformHandle = entity.get<EntityTransformHandle>();
	return transformHandle.transformSuballocation.GetOffset() / sizeof(math::Transform3f);
}

void RenderScene::InitializeRenderSystem(RenderSystem& system)
{
	SPT_PROFILER_FUNCTION();

	const RenderSceneEntity systemEntity = m_registry.create();
	system.Initialize(*this, RenderSceneEntityHandle(m_registry, systemEntity));
}

void RenderScene::DeinitializeRenderSystem(RenderSystem& system)
{
	system.Deinitialize(*this);
}

} // spt::rsc
