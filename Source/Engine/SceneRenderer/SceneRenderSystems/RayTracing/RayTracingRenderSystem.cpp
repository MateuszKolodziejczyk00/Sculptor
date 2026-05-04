#include "RayTracingRenderSystem.h"
#include "RenderScene.h"
#include "RayTracingSceneTypes.h"
#include "Types/AccelerationStructure.h"
#include "ResourcesManager.h"
#include "GPUApi.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Types/RenderContext.h"
#include "Materials/MaterialsRenderingCommon.h"
#include "MaterialsSubsystem.h"
#include "RenderSceneConstants.h"
#include "RayTracing/RayTracingGeometry.h"


namespace spt::rsc
{

SPT_REGISTER_SCENE_RENDER_SYSTEM(RayTracingRenderSystem);


RayTracingRenderSystem::RayTracingRenderSystem(RenderScene& owningScene)
	: Super(owningScene)
	, m_isTLASDirty(false)
{ }

void RayTracingRenderSystem::Update(const SceneUpdateContext& context)
{
	Super::Update(context);

	m_isTLASDirty = false;

	// We should update TLAS every frame if any of the objects has been moved or updated
	// The problem is that Nsight Graphics is crashing if we capture frame with TLAS build, so for now we will update TLAS only once
	// TODO: Refactor to update TLAS every frame if any of the objects has been moved or updated
	if (!m_tlas)
	{
		UpdateTLAS();
	}
}

void RayTracingRenderSystem::UpdateGPUSceneData(RenderSceneConstants& sceneData)
{
	RTSceneData rtSceneData;
	rtSceneData.tlas        = m_tlas;
	rtSceneData.rtInstances = m_rtInstancesDataBuffer->GetFullView();

	sceneData.rtScene = rtSceneData;
}

const lib::SharedPtr<rdr::Buffer>& RayTracingRenderSystem::GetRTInstancesDataBuffer() const
{
	return m_rtInstancesDataBuffer;
}

Bool RayTracingRenderSystem::IsTLASDirty() const
{
	return m_isTLASDirty;
}

void RayTracingRenderSystem::UpdateTLAS()
{
	SPT_PROFILER_FUNCTION();

	RenderScene& scene = GetOwningScene();

	rhi::TLASDefinition tlasDefinition;

	lib::DynamicArray<RTInstanceData> rtInstances;

	const auto processRTInstance = [&](RTInstanceHandle handle, const RTInstance& instance)
	{
		const RayTracingGeometryProvider& rtGeo = instance.rtGeometry;

		const lib::Span<const RayTracingGeometryDefinition> rtGeometries = rtGeo.GetRayTracingGeometries();

		MaterialSlotsChunk* materialsSlots = scene.materials.slots.Get(instance.materialSlots);
		Uint32 currentSlotIdxInChunk = 0u;

		const RenderInstance* renderInstance = scene.GetInstances().Get(instance.instance);
		SPT_CHECK(renderInstance != nullptr);

		const rhi::TLASInstanceDefinition::TransformMatrix transformMatrix = renderInstance->transform.matrix().topLeftCorner<3, 4>();

		for(SizeType idx = 0; idx < rtGeometries.size(); ++idx)
		{
			const RayTracingGeometryDefinition& rtGeometry   = rtGeometries[idx];
			const ecs::EntityHandle material                 = materialsSlots->slots[currentSlotIdxInChunk++];
			const mat::MaterialProxyComponent& materialProxy = material.get<const mat::MaterialProxyComponent>();

			if(materialProxy.SupportsRayTracing())
			{
				EMaterialRTFlags materialRTFlags = EMaterialRTFlags::None;
				if (materialProxy.params.doubleSided)
				{
					lib::AddFlag(materialRTFlags, EMaterialRTFlags::DoubleSided);
				}

				RTInstanceData& rtInstance = rtInstances.emplace_back();
				rtInstance.entity                 = GetInstanceGPUDataPtr(instance.instance);
				rtInstance.materialDataHandle     = materialProxy.GetMaterialDataHandle();
				rtInstance.metarialRTFlags        = static_cast<Uint16>(materialRTFlags);
				rtInstance.indicesDataUGBOffset   = rtGeometry.indicesDataUGBOffset;
				rtInstance.locationsDataUGBOffset = rtGeometry.locationsDataUGBOffset;
				rtInstance.uvsDataUGBOffset       = rtGeometry.uvsDataUGBOffset;
				rtInstance.normalsDataUGBOffset   = rtGeometry.normalsDataUGBOffset;
				rtInstance.uvsMin                 = rtGeometry.uvsMin;
				rtInstance.uvsRange               = rtGeometry.uvsRange;

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

				if (materialProxy.params.doubleSided)
				{
					lib::AddFlag(tlasInstance.flags, rhi::ETLASInstanceFlags::FacingCullDisable);
				}
			}

			if (currentSlotIdxInChunk >= materialsSlots->slots.size())
			{
				currentSlotIdxInChunk = 0u;
				materialsSlots = scene.materials.slots.Get(instance.materialSlots);
				SPT_CHECK(materialsSlots != nullptr);
			}
		}
	};

	scene.rt.instances.ForEach(processRTInstance);

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

		rdr::GPUApi::GetDeviceQueuesManager().Submit(workload, lib::Flags(rdr::EGPUWorkloadSubmitFlags::MemoryTransfersWait, rdr::EGPUWorkloadSubmitFlags::CorePipe));

		m_tlas->ReleaseInstancesBuildData();
	}

	m_isTLASDirty = true;
}

lib::SharedPtr<rdr::Buffer> RayTracingRenderSystem::BuildRTInstancesBuffer(const lib::DynamicArray<RTInstanceData>& instances) const
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
