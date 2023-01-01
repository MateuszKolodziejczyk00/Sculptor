#include "RenderScene.h"
#include "ResourcesManager.h"
#include "BufferUtilities.h"

namespace spt::rsc
{

RenderScene::RenderScene()
	: m_basePassRenderInstances(RENDERER_RESOURCE_NAME("BasePassInstancesList"), 1024)
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

rsc::RenderSceneEntity RenderScene::CreateEntity(const RenderInstanceData& instanceData)
{
	SPT_PROFILER_FUNCTION();

	const rsc::RenderSceneEntity entity(m_registry.create());

	const rhi::SuballocationDefinition suballocationDef(sizeof(math::Transform3f), sizeof(math::Transform3f), rhi::EBufferSuballocationFlags::PreferFasterAllocation);
	const rhi::RHISuballocation entityTransformSuballocation = m_instanceTransforms->GetRHI().CreateSuballocation(suballocationDef);

	m_registry.emplace<EntityTransformHandle>(entity, entityTransformSuballocation);

	const Byte* transformData = reinterpret_cast<const Byte*>(instanceData.transfrom.data());
	gfx::UploadDataToBuffer(lib::Ref(m_instanceTransforms), entityTransformSuballocation.GetOffset(), transformData, sizeof(math::Transform3f));

	return entity;
}

void RenderScene::DestroyEntity(RenderSceneEntity entity)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK_NO_ENTRY_MSG("TODO");
}

BasePassRenderInstancesList& RenderScene::GetBasePassRenderInstancesList()
{
	return m_basePassRenderInstances;
}

const BasePassRenderInstancesList& RenderScene::GetBasePassRenderInstancesList() const
{
	return m_basePassRenderInstances;
}

} // spt::rsc
