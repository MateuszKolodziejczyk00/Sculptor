#include "RenderScene.h"
#include "ResourcesManager.h"
#include "BufferUtilities.h"
#include "JobSystem/JobSystem.h"

namespace spt::rsc
{

RenderScene::RenderScene()
	: m_renderEntitiesBuffer(CreateInstancesBuffer())
	, m_renderSceneDS(CreateRenderSceneDS())
{ }

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

	const rhi::SuballocationDefinition suballocationDef(sizeof(RenderEntityGPUData), sizeof(RenderEntityGPUData), rhi::EBufferSuballocationFlags::PreferFasterAllocation);
	const rhi::RHISuballocation entityGPUDataSuballocation = m_renderEntitiesBuffer->GetRHI().CreateSuballocation(suballocationDef);
	SPT_CHECK_MSG(entityGPUDataSuballocation.IsValid(), "Failed to allocate data for instance!");

	entity.emplace<EntityGPUDataHandle>(entityGPUDataSuballocation);

	RenderEntityGPUData entityGPUData;
	entityGPUData.transform = instanceData.transfrom.matrix();

	const Byte* entityDataPtr = reinterpret_cast<const Byte*>(&entityGPUData);
	gfx::UploadDataToBuffer(m_renderEntitiesBuffer, entityGPUDataSuballocation.GetOffset(), entityDataPtr, sizeof(RenderEntityGPUData));

	return entity;
}

void RenderScene::DestroyEntity(RenderSceneEntityHandle entity)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK_NO_ENTRY_MSG("TODO");
}

Uint64 RenderScene::GetEntityIdx(RenderSceneEntityHandle entity) const
{
	SPT_PROFILER_FUNCTION();

	const EntityGPUDataHandle& entityDataHandle = entity.get<EntityGPUDataHandle>();
	return entityDataHandle.GetEntityIdx();
}

const lib::DynamicArray<lib::UniquePtr<RenderSystem>>& RenderScene::GetRenderSystems() const
{
	return m_renderSystems.GetRenderSystems();
}

RenderSceneEntityHandle RenderScene::CreateViewEntity()
{
	const RenderSceneEntity entityID = m_registry.create();
	return RenderSceneEntityHandle(m_registry, entityID);
}

const lib::SharedRef<rdr::Buffer>& RenderScene::GetRenderEntitiesBuffer() const
{
	return m_renderEntitiesBuffer;
}

const lib::SharedRef<RenderSceneDS>& RenderScene::GetRenderSceneDS() const
{
	return m_renderSceneDS;
}

lib::SharedRef<rdr::Buffer> RenderScene::CreateInstancesBuffer() const
{
	rhi::RHIAllocationInfo renderEntitiesAllocationInfo;
	renderEntitiesAllocationInfo.memoryUsage = rhi::EMemoryUsage::GPUOnly;

	const Uint64 maxInstancesNum = 4096;

	rhi::BufferDefinition renderEntitiesBufferDef;
	renderEntitiesBufferDef.size = maxInstancesNum * sizeof(RenderEntityGPUData);
	renderEntitiesBufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
	renderEntitiesBufferDef.flags = rhi::EBufferFlags::WithVirtualSuballocations;

	return rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("RenderEntitiesGPUDataBuffer"), renderEntitiesBufferDef, renderEntitiesAllocationInfo);
}

lib::SharedRef<RenderSceneDS> RenderScene::CreateRenderSceneDS() const
{
	const lib::SharedRef<RenderSceneDS> sceneDS = rdr::ResourcesManager::CreateDescriptorSetState<RenderSceneDS>(RENDERER_RESOURCE_NAME("RenderSceneDS"), rdr::EDescriptorSetStateFlags::Persistent);
	sceneDS->u_renderEntitiesData = m_renderEntitiesBuffer->CreateFullView();
	return sceneDS;
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
