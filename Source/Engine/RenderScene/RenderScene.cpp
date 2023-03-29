#include "RenderScene.h"
#include "ResourcesManager.h"
#include "Transfers/UploadUtils.h"
#include "JobSystem/JobSystem.h"
#include "EngineFrame.h"

namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// TransformComponent ============================================================================

TransformComponent::TransformComponent()
	: m_transform(math::Affine3f::Identity())
	, m_uniformScale(1.f)
{ }

TransformComponent::TransformComponent(const math::Affine3f& transform)
{
	SetTransform(transform);
}

void TransformComponent::SetTransform(const math::Affine3f& newTransform)
{
	m_transform = newTransform;

	const math::Matrix4f& transformMatrix = m_transform.matrix();

	const Real32 scaleX2 = transformMatrix.row(0).head<3>().squaredNorm();
	const Real32 scaleY2 = transformMatrix.row(1).head<3>().squaredNorm();
	const Real32 scaleZ2 = transformMatrix.row(2).head<3>().squaredNorm();

	const Real32 maxScale = std::max(std::max(scaleX2, scaleY2), scaleZ2);
	m_uniformScale = std::sqrt(maxScale);
}

const math::Affine3f& TransformComponent::GetTransform() const
{
	return m_transform;
}

Real32 TransformComponent::GetUniformScale() const
{
	return m_uniformScale;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RenderScene ===================================================================================

RenderScene::RenderScene()
	: m_renderEntitiesBuffer(CreateInstancesBuffer())
	, m_renderSceneDS(CreateRenderSceneDS())
{ }

RenderSceneRegistry& RenderScene::GetRegistry()
{
	return m_registry;
}

const RenderSceneRegistry& RenderScene::GetRegistry() const
{
	return m_registry;
}

void RenderScene::Update()
{
	SPT_PROFILER_FUNCTION();

	const auto& primitiveSystems = m_primitiveSystems.GetSystems();

	js::ParallelForEach("Update Primitive Systems",
						primitiveSystems,
						[](const lib::SharedPtr<PrimitivesSystem>& system)
						{
							system->Update();
						},
						js::EJobPriority::High,
						js::EJobFlags::Inline);

	const lib::DynamicArray<RenderSystem*>& renderSystemsWithUpdate = m_renderSystems.GetRenderSystemsWithUpdate();
	if (!renderSystemsWithUpdate.empty())
	{
		js::ParallelForEach("Update Render Systems",
							renderSystemsWithUpdate,
							[this](RenderSystem* system)
							{
								system->Update(*this);
							},
							js::EJobPriority::High,
							js::EJobFlags::Inline);
	}

	GPUSceneFrameData frameData;
	frameData.deltaTime	= engn::GetRenderingFrame().GetDeltaTime();
	frameData.time		= engn::GetRenderingFrame().GetTime();
	m_renderSceneDS->u_gpuSceneFrameConstants = frameData;
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

	const rhi::SuballocationDefinition suballocationDef(sizeof(RenderEntityGPUData), sizeof(RenderEntityGPUData), rhi::EBufferSuballocationFlags::PreferFasterAllocation);
	const rhi::RHISuballocation entityGPUDataSuballocation = m_renderEntitiesBuffer->GetRHI().CreateSuballocation(suballocationDef);
	SPT_CHECK_MSG(entityGPUDataSuballocation.IsValid(), "Failed to allocate data for instance!");

	entity.emplace<EntityGPUDataHandle>(entityGPUDataSuballocation);

	RenderEntityGPUData entityGPUData;
	entityGPUData.transform		= transformComp.GetTransform().matrix();
	entityGPUData.uniformScale	= transformComp.GetUniformScale();

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

const lib::DynamicArray<lib::SharedRef<RenderSystem>>& RenderScene::GetRenderSystems() const
{
	return m_renderSystems.GetRenderSystems();
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

	system.Initialize(*this);
}

void RenderScene::DeinitializeRenderSystem(RenderSystem& system)
{
	SPT_PROFILER_FUNCTION();

	system.Deinitialize(*this);
}

} // spt::rsc
