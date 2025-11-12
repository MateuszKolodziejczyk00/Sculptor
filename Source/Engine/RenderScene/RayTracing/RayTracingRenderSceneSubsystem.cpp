#include "RayTracingRenderSceneSubsystem.h"
#include "RenderScene.h"
#include "RayTracingSceneTypes.h"
#include "Types/AccelerationStructure.h"
#include "ResourcesManager.h"
#include "Renderer.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Types/RenderContext.h"
#include "Materials/MaterialsRenderingCommon.h"
#include "MaterialsSubsystem.h"


namespace spt::rsc
{

RayTracingRenderSceneSubsystem::RayTracingRenderSceneSubsystem(RenderScene& owningScene)
	: Super(owningScene)
	, m_isTLASDirty(false)
{ }

void RayTracingRenderSceneSubsystem::Update()
{
	Super::Update();

	m_isTLASDirty = false;

	// We should update TLAS every frame if any of the objects has been moved or updated
	// The problem is that Nsight Graphics is crashing if we capture frame with TLAS build, so for now we will update TLAS only once
	// TODO: Refactor to update TLAS every frame if any of the objects has been moved or updated
	if (!m_tlas)
	{
		UpdateTLAS();
	}
}

const lib::SharedPtr<rdr::Buffer>& RayTracingRenderSceneSubsystem::GetRTInstancesDataBuffer() const
{
	return m_rtInstancesDataBuffer;
}

const lib::MTHandle<SceneRayTracingDS>& RayTracingRenderSceneSubsystem::GetSceneRayTracingDS() const
{
	return m_sceneRayTracingDS;
}

Bool RayTracingRenderSceneSubsystem::IsTLASDirty() const
{
	return m_isTLASDirty;
}

void RayTracingRenderSceneSubsystem::UpdateTLAS()
{
	SPT_PROFILER_FUNCTION();

	const RenderSceneRegistry& sceneRegistry = GetOwningScene().GetRegistry();

	rhi::TLASDefinition tlasDefinition;

	const auto rayTracedObjectsEntities = sceneRegistry.view<const EntityGPUDataHandle, const TransformComponent, const rsc::MaterialSlotsComponent, const RayTracingGeometryProviderComponent>();
	const SizeType rayTracedEntitiesNum = static_cast<SizeType>(rayTracedObjectsEntities.size_hint() * 2.2f);
	tlasDefinition.instances.reserve(rayTracedEntitiesNum);

	lib::DynamicArray<RTInstanceData> rtInstances;
	rtInstances.reserve(rayTracedObjectsEntities.size_hint());

	for (const auto& [entity, gpuEntity, transform, materialsSlots, rtGeoProvider] : rayTracedObjectsEntities.each())
	{
		const RayTracingGeometryComponent& rayTracingGeoComp = rtGeoProvider.entity.get<const RayTracingGeometryComponent>();

		const rhi::TLASInstanceDefinition::TransformMatrix transformMatrix = transform.GetTransform().matrix().topLeftCorner<3, 4>();

		SPT_CHECK(rayTracingGeoComp.geometries.size() == materialsSlots.slots.size());

		for(SizeType idx = 0; idx < rayTracingGeoComp.geometries.size(); ++idx)
		{
			const RayTracingGeometryDefinition& rtGeometry		= rayTracingGeoComp.geometries[idx];
			const ecs::EntityHandle material					= materialsSlots.slots[idx];
			const mat::MaterialProxyComponent& materialProxy	= material.get<const mat::MaterialProxyComponent>();

			if(materialProxy.SupportsRayTracing())
			{
				RTInstanceData& rtInstance = rtInstances.emplace_back();
				rtInstance.entityIdx      = gpuEntity.GetGPUDataPtr().GetBufferElementIdx();
				rtInstance.materialDataID = materialProxy.GetMaterialDataID();
				rtInstance.geometryDataID = rtGeometry.geometryDataID;

				mat::RTHitGroupPermutation hitGroupPermutation;
				hitGroupPermutation.SHADER = materialProxy.params.shader;
				const Uint32 hitGroupIdx = mat::MaterialsSubsystem::Get().GetRTHitGroupIdx(hitGroupPermutation);

				ETLASGeometryMask mask = ETLASGeometryMask::Opaque;
				if (materialProxy.params.transparent)
				{
					mask = ETLASGeometryMask::Transparent;
				}

				rhi::TLASInstanceDefinition& tlasInstance = tlasDefinition.instances.emplace_back();
				tlasInstance.transform       = transformMatrix;
				tlasInstance.blasAddress     = rtGeometry.blas->GetRHI().GetDeviceAddress();
				tlasInstance.customIdx       = static_cast<Uint32>(rtInstances.size() - 1);
				tlasInstance.sbtRecordOffset = hitGroupIdx;
				tlasInstance.mask            = static_cast<Uint32>(mask);

				if (!materialProxy.params.customOpacity)
				{
					lib::AddFlag(tlasInstance.flags, rhi::ETLASInstanceFlags::ForceOpaque);
				}
			}
		}
	}

	m_rtInstancesDataBuffer = BuildRTInstancesBuffer(rtInstances);

	if (!tlasDefinition.instances.empty())
	{
		m_tlas = rdr::ResourcesManager::CreateTLAS(RENDERER_RESOURCE_NAME("Scene TLAS"), tlasDefinition);

		const rhi::BufferDefinition scratchBufferDef(m_tlas->GetRHI().GetBuildScratchSize(), lib::Flags(rhi::EBufferUsage::DeviceAddress, rhi::EBufferUsage::Storage));
		const lib::SharedRef<rdr::Buffer> scratchBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("TLASBuilderScratchBuffer"), scratchBufferDef, rhi::EMemoryUsage::GPUOnly);

		// build TLAS
		lib::SharedRef<rdr::RenderContext> context = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("BuildTLASContext"), rhi::ContextDefinition());

		const rhi::CommandBufferDefinition cmdBufferDef(rhi::EDeviceCommandQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Low);
		lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::ResourcesManager::CreateCommandRecorder(RENDERER_RESOURCE_NAME("BuildTLASCmdBuffer"),
																									 context,
																									 cmdBufferDef);

		recorder->BuildTLAS(lib::Ref(m_tlas), scratchBuffer, 0, lib::Ref(m_tlas->GetInstancesBuildDataBuffer()));

		const lib::SharedRef<rdr::GPUWorkload> workload = recorder->FinishRecording();

		rdr::Renderer::GetDeviceQueuesManager().Submit(workload, lib::Flags(rdr::EGPUWorkloadSubmitFlags::MemoryTransfersWait, rdr::EGPUWorkloadSubmitFlags::CorePipe));

		m_tlas->ReleaseInstancesBuildData();

		m_sceneRayTracingDS = rdr::ResourcesManager::CreateDescriptorSetState<SceneRayTracingDS>(RENDERER_RESOURCE_NAME("Scene Ray Tracing DS"));
		m_sceneRayTracingDS->u_sceneTLAS   = lib::Ref(m_tlas);
		m_sceneRayTracingDS->u_rtInstances = m_rtInstancesDataBuffer->GetFullView();
	}

	m_isTLASDirty = true;
}

lib::SharedPtr<rdr::Buffer> RayTracingRenderSceneSubsystem::BuildRTInstancesBuffer(const lib::DynamicArray<RTInstanceData>& instances) const
{
	if (instances.empty())
	{
		return lib::SharedPtr<rdr::Buffer>();
	}

	rhi::BufferDefinition bufferDef;
	bufferDef.size	= instances.size() * sizeof(rdr::HLSLStorage<RTInstanceData>);
	bufferDef.usage	= rhi::EBufferUsage::Storage;

	lib::SharedPtr<rdr::Buffer> instancesDataBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("RT Instances Data"), bufferDef, rhi::EMemoryUsage::CPUToGPU);

	rhi::RHIMappedBuffer<rdr::HLSLStorage<RTInstanceData>> mappedInstancesData(instancesDataBuffer->GetRHI());

	for (SizeType instanceIdx = 0; instanceIdx < instances.size(); ++instanceIdx)
	{
		mappedInstancesData[instanceIdx] = instances[instanceIdx];
	}

	return instancesDataBuffer;
}

} // spt::rsc
