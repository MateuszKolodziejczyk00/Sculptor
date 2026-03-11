#include "RenderScene.h"
#include "ResourcesManager.h"
#include "Utils/TransfersUtils.h"
#include "EngineFrame.h"


namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RenderScene ===================================================================================

RenderScene::RenderScene()
	: m_renderEntitiesBuffer(CreateInstancesBuffer())
{
	ecs::InitializeRegistry(m_registry);
}

RenderSceneRegistry& RenderScene::GetRegistry()
{
	return m_registry;
}

const RenderSceneRegistry& RenderScene::GetRegistry() const
{
	return m_registry;
}

void RenderScene::BeginFrame(const engn::FrameContext& frame)
{
	SPT_CHECK(!m_currentFrame);
	m_currentFrame = frame.shared_from_this();
}

void RenderScene::EndFrame()
{
	SPT_CHECK(!!m_currentFrame);
	m_currentFrame.reset();
}

const engn::FrameContext& RenderScene::GetCurrentFrameRef() const
{
	SPT_CHECK_MSG(m_currentFrame, "No current frame!");
	return *m_currentFrame;
}

RenderSceneEntityHandle RenderScene::CreateEntity()
{
	const RenderSceneEntity entityID = m_registry.create();
	return RenderSceneEntityHandle(m_registry, entityID);
}

RenderSceneEntityHandle RenderScene::CreateEntity(const RenderInstanceData& instanceData)
{
	SPT_PROFILER_FUNCTION();

	const RenderSceneEntityHandle entity = CreateEntity();

	const TransformComponent& transformComp = entity.emplace<TransformComponent>(instanceData.transformComp);

	const rhi::VirtualAllocationDefinition suballocationDef(sizeof(rdr::HLSLStorage<RenderEntityGPUData>), sizeof(rdr::HLSLStorage<RenderEntityGPUData>), rhi::EVirtualAllocationFlags::PreferFasterAllocation);
	const rhi::RHIVirtualAllocation entityGPUDataSuballocation = m_renderEntitiesBuffer->GetRHI().CreateSuballocation(suballocationDef);
	SPT_CHECK_MSG(entityGPUDataSuballocation.IsValid(), "Failed to allocate data for instance!");

	entity.emplace<EntityGPUDataHandle>(entityGPUDataSuballocation);

	RenderEntityGPUData entityGPUData;
	entityGPUData.transform		= transformComp.GetTransform().matrix();
	entityGPUData.uniformScale	= transformComp.GetUniformScale();

	const Byte* entityDataPtr = reinterpret_cast<const Byte*>(&entityGPUData);
	rdr::UploadDataToBuffer(m_renderEntitiesBuffer, entityGPUDataSuballocation.GetOffset(), entityDataPtr, sizeof(RenderEntityGPUData));

	return entity;
}

void RenderScene::DestroyEntity(RenderSceneEntityHandle entity)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK_NO_ENTRY_MSG("TODO");
}

const lib::SharedRef<rdr::Buffer>& RenderScene::GetRenderEntitiesBuffer() const
{
	return m_renderEntitiesBuffer;
}

lib::SharedRef<rdr::Buffer> RenderScene::CreateInstancesBuffer() const
{
	rhi::RHIAllocationInfo renderEntitiesAllocationInfo;
	renderEntitiesAllocationInfo.memoryUsage = rhi::EMemoryUsage::GPUOnly;

	const Uint64 maxInstancesNum = 8192u;

	rhi::BufferDefinition renderEntitiesBufferDef;
	renderEntitiesBufferDef.size = maxInstancesNum * sizeof(RenderEntityGPUData);
	renderEntitiesBufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
	renderEntitiesBufferDef.flags = rhi::EBufferFlags::WithVirtualSuballocations;

	return rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("RenderEntitiesGPUDataBuffer"), renderEntitiesBufferDef, renderEntitiesAllocationInfo);
}

} // spt::rsc
