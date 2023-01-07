#include "RenderScene.h"
#include "ResourcesManager.h"
#include "BufferUtilities.h"
#include "JobSystem/JobSystem.h"

namespace spt::rsc
{

RenderScene::RenderScene()
{
	rhi::RHIAllocationInfo transformsAllocationInfo;
	transformsAllocationInfo.memoryUsage = rhi::EMemoryUsage::GPUOnly;
	transformsAllocationInfo.allocationFlags = rhi::EAllocationFlags::CreateDedicatedAllocation;

	rhi::BufferDefinition transformsBufferDef;
	transformsBufferDef.size = 1024 * sizeof(math::Affine3f);
	transformsBufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
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

void RenderScene::Update()
{
	SPT_PROFILER_FUNCTION();

	const auto& systems = m_primitiveSystems.GetSystems();

	js::ParallelForEach("Update Render Systems",
						systems,
						[](const lib::UniquePtr<PrimitivesSystem>& system)
						{
							system->Update();
						},
						js::EJobPriority::High,
						js::EJobFlags::Inline);
}

RenderSceneEntityHandle RenderScene::CreateEntity(const RenderInstanceData& instanceData)
{
	SPT_PROFILER_FUNCTION();

	const RenderSceneEntity entityID = m_registry.create();
	const RenderSceneEntityHandle entity(m_registry, entityID);

	const rhi::SuballocationDefinition suballocationDef(sizeof(math::Affine3f), sizeof(math::Affine3f), rhi::EBufferSuballocationFlags::PreferFasterAllocation);
	const rhi::RHISuballocation entityTransformSuballocation = m_instanceTransforms->GetRHI().CreateSuballocation(suballocationDef);

	entity.emplace<EntityTransformHandle>(entityTransformSuballocation);

	const Byte* transformData = reinterpret_cast<const Byte*>(instanceData.transfrom.data());
	gfx::UploadDataToBuffer(lib::Ref(m_instanceTransforms), entityTransformSuballocation.GetOffset(), transformData, sizeof(math::Affine3f));

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
	return transformHandle.transformSuballocation.GetOffset() / sizeof(math::Affine3f);
}

const lib::DynamicArray<lib::UniquePtr<RenderSystem>>& RenderScene::GetRenderSystems() const
{
	return m_renderSystems.GetRenderSystems();
}

void RenderScene::InitializeRenderSystem(RenderSystem& system)
{
	SPT_PROFILER_FUNCTION();

	const RenderSceneEntity systemEntity = m_registry.create();
	system.Initialize(*this, RenderSceneEntityHandle(m_registry, systemEntity));
}

void RenderScene::DeinitializeRenderSystem(RenderSystem& system)
{
	m_registry.destroy(system.GetSystemEntity());
	system.Deinitialize(*this);
}

} // spt::rsc
