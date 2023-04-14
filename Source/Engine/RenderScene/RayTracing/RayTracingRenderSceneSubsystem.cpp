#include "RayTracingRenderSceneSubsystem.h"
#include "RenderScene.h"
#include "RayTracingSceneTypes.h"
#include "Types/AccelerationStructure.h"
#include "ResourcesManager.h"
#include "Renderer.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Types/RenderContext.h"
#include "Materials/MaterialTypes.h"

namespace spt::rsc
{

RayTracingRenderSceneSubsystem::RayTracingRenderSceneSubsystem(RenderScene& owningScene)
	: Super(owningScene)
{ }

void RayTracingRenderSceneSubsystem::Update()
{
	Super::Update();

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

void RayTracingRenderSceneSubsystem::UpdateTLAS()
{
	SPT_PROFILER_FUNCTION();

	const RenderSceneRegistry& sceneRegistry = GetOwningScene().GetRegistry();

	rhi::TLASDefinition tlasDefinition;

	const auto rayTracedObjectsEntities = sceneRegistry.view<EntityGPUDataHandle, TransformComponent, MaterialsDataComponent, RayTracingGeometryProviderComponent>();
	const SizeType rayTracedEntitiesNum = static_cast<SizeType>(rayTracedObjectsEntities.size_hint() * 2.2f);
	tlasDefinition.instances.reserve(rayTracedEntitiesNum);

	lib::DynamicArray<RTInstanceData> rtInstances;
	rtInstances.reserve(rayTracedObjectsEntities.size_hint());

	for (const auto& [entity, gpuEntity, transform, materialsData, rtGeoProvider] : rayTracedObjectsEntities.each())
	{
		const RayTracingGeometryComponent& rayTracingGeoComp = rtGeoProvider.entity.get<RayTracingGeometryComponent>();

		const rhi::TLASInstanceDefinition::TransformMatrix transformMatrix = transform.GetTransform().matrix().topLeftCorner<3, 4>();

		SPT_CHECK(rayTracingGeoComp.geometries.size() == materialsData.materials.size());
		
		for(SizeType idx = 0; idx < rayTracingGeoComp.geometries.size(); ++idx)
		{
			const RayTracingGeometryDefinition& rtGeometry	= rayTracingGeoComp.geometries[idx];
			const RenderingDataEntityHandle material		= materialsData.materials[idx];
			const MaterialCommonData& materialData			= material.get<MaterialCommonData>();

			RTInstanceData& rtInstance = rtInstances.emplace_back();
			rtInstance.entityIdx				= gpuEntity.GetEntityIdx();
			rtInstance.materialDataOffset		= static_cast<Uint32>(materialData.materialDataSuballocation.GetOffset());
			rtInstance.geometryDataID			= rtGeometry.geometryDataID;

			rhi::TLASInstanceDefinition& tlasInstance = tlasDefinition.instances.emplace_back();
			tlasInstance.transform		= transformMatrix;
			tlasInstance.blasAddress	= rtGeometry.blas->GetRHI().GetDeviceAddress();
			tlasInstance.customIdx		= static_cast<Uint32>(rtInstances.size() - 1);
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
		lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::Renderer::StartRecordingCommands();

		recorder->BuildTLAS(lib::Ref(m_tlas), scratchBuffer, 0, lib::Ref(m_tlas->GetInstancesBuildDataBuffer()));

		rdr::CommandsRecordingInfo recordingInfo;
		recordingInfo.commandsBufferName = RENDERER_RESOURCE_NAME("BuildTLASCmdBuffer");
		recordingInfo.commandBufferDef = rhi::CommandBufferDefinition(rhi::ECommandBufferQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Low);
		recorder->RecordCommands(context, recordingInfo, rhi::CommandBufferUsageDefinition(rhi::ECommandBufferBeginFlags::OneTimeSubmit));

		rdr::CommandsSubmitBatch submitBatch;
		submitBatch.recordedCommands.emplace_back(std::move(recorder));
		rdr::Renderer::SubmitCommands(rhi::ECommandBufferQueueType::Graphics, std::move(submitBatch));

		m_tlas->ReleaseInstancesBuildData();
	}
}

lib::SharedPtr<rdr::Buffer> RayTracingRenderSceneSubsystem::BuildRTInstancesBuffer(const lib::DynamicArray<RTInstanceData>& instances) const
{
	if (instances.empty())
	{
		return lib::SharedPtr<rdr::Buffer>();
	}

	rhi::BufferDefinition bufferDef;
	bufferDef.size	= instances.size() * sizeof(RTInstanceData);
	bufferDef.usage	= rhi::EBufferUsage::Storage;

	lib::SharedPtr<rdr::Buffer> instancesDataBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("RT Instances Data"), bufferDef, rhi::EMemoryUsage::CPUToGPU);

	rhi::RHIMappedBuffer<RTInstanceData> mappedInstancesData(instancesDataBuffer->GetRHI());

	for (SizeType instanceIdx = 0; instanceIdx < instances.size(); ++instanceIdx)
	{
		mappedInstancesData[instanceIdx] = instances[instanceIdx];
	}

	return instancesDataBuffer;
}

} // spt::rsc
