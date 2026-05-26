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


RayTracingRenderSystem::RayTracingRenderSystem(lib::MemoryArena& arena, RenderScene& owningScene)
	: Super(arena, owningScene)
{
	m_supportedStages = ERenderStage::PreRendering;

	const Uint32 maxInstancesNum = RTScene::maxInstancesNum;

	const rhi::TLASDefinition tlasDef
	{
		.maxInstancesNum = maxInstancesNum
	};

	m_tlas = rdr::ResourcesManager::CreateTLAS(RENDERER_RESOURCE_NAME("Scene TLAS"), tlasDef);

	rhi::BufferDefinition instancesBufferDef;
	instancesBufferDef.size  = maxInstancesNum * sizeof(rdr::HLSLStorage<RTInstanceData>);
	instancesBufferDef.usage = lib::Flags(rhi::EBufferUsage::DeviceAddress, rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
	m_rtInstancesDataBuffer        = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("TLAS Instances Data Buffer"), instancesBufferDef, rhi::EMemoryUsage::GPUOnly);
	m_rtInstancesDataStagingBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("TLAS Instances Data Staging Buffer"), instancesBufferDef, rhi::EMemoryUsage::CPUToGPU);

	rhi::BufferDefinition instancesDefsBufferDef;
	instancesDefsBufferDef.size  = rhi::RHIASUtils::GetInstancesBufferSize(maxInstancesNum);
	instancesDefsBufferDef.usage = lib::Flags(rhi::EBufferUsage::DeviceAddress, rhi::EBufferUsage::ASBuildInputReadOnly);
	m_instancesDefsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("TLAS Instances Definitions Buffer"), instancesDefsBufferDef, rhi::EMemoryUsage::CPUToGPU);
}

void RayTracingRenderSystem::UpdateGPUSceneData(const SceneUpdateContext& context, RenderSceneConstants& sceneData)
{
	RTSceneData rtSceneData;
	rtSceneData.tlas        = m_tlas;
	rtSceneData.rtInstances = m_rtInstancesDataBuffer->GetFullView();

	sceneData.rtScene = rtSceneData;
}

void RayTracingRenderSystem::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const lib::DynamicPushArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings)
{
	SPT_PROFILER_FUNCTION();

	Super::RenderPerFrame(graphBuilder, rendererInterface, renderScene, viewSpecs, settings);

	ViewRenderingSpec* mainView = *viewSpecs.begin();
	SPT_CHECK(mainView != nullptr);

	mainView->GetRenderViewEntry(ERenderViewEntry::BuildTLAS).AddRawMember(this, &RayTracingRenderSystem::OnBuildTLAS);
}

void RayTracingRenderSystem::OnBuildTLAS(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context)
{
	SPT_PROFILER_FUNCTION();

	rhi::RHIMappedBuffer<rdr::HLSLStorage<RTInstanceData>> rtInstances(m_rtInstancesDataStagingBuffer->GetRHI());
	rhi::RHIMappedByteBuffer rtInstancesDefs(m_instancesDefsBuffer->GetRHI());

	Uint32 currentInstanceIdx = 0u;

	const auto processRTInstance = [&](RTInstanceHandle handle, const RTInstance& instance)
	{
		const RayTracingGeometryProvider& rtGeo = instance.rtGeometry;

		const lib::Span<const RayTracingGeometryDefinition> rtGeometries = rtGeo.GetRayTracingGeometries();

		MaterialSlotsChunk* materialsSlots = scene.materials.slots.Get(instance.materialSlots);
		Uint32 currentSlotIdxInChunk = 0u;

		const RenderInstance* renderInstance = scene.GetInstances().Get(instance.instance);

		const math::Matrix4f transform = renderInstance ? renderInstance->transform.matrix() : math::Matrix4f::Identity();
		const rhi::TLASInstanceDefinition::TransformMatrix transformMatrix = transform.topLeftCorner<3, 4>();

		for(SizeType idx = 0; idx < rtGeometries.size(); ++idx)
		{
			const RayTracingGeometryDefinition& rtGeometry   = rtGeometries[idx];
			const ecs::EntityHandle material                 = materialsSlots->slots[currentSlotIdxInChunk++];
			const mat::MaterialProxyComponent& materialProxy = material.get<const mat::MaterialProxyComponent>();

			if(materialProxy.SupportsRayTracing() && !!rtGeometry.blas)
			{
				const Uint32 instanceIdx = currentInstanceIdx++;
				SPT_CHECK(instanceIdx < RTScene::maxInstancesNum);

				EMaterialRTFlags materialRTFlags = EMaterialRTFlags::None;
				if (materialProxy.params.doubleSided)
				{
					lib::AddFlag(materialRTFlags, EMaterialRTFlags::DoubleSided);
				}

				RTInstanceData rtInstance;
				rtInstance.entity                 = GetInstanceGPUDataPtr(instance.instance);
				rtInstance.materialDataHandle     = materialProxy.GetMaterialDataHandle();
				rtInstance.metarialRTFlags        = static_cast<Uint16>(materialRTFlags);
				rtInstance.instanceType           = static_cast<Uint16>(instance.type);
				rtInstance.indicesDataUGBOffset   = rtGeometry.indicesDataUGBOffset;
				rtInstance.locationsDataUGBOffset = rtGeometry.locationsDataUGBOffset;
				rtInstance.uvsDataUGBOffset       = rtGeometry.uvsDataUGBOffset;
				rtInstance.normalsDataUGBOffset   = rtGeometry.normalsDataUGBOffset;
				rtInstance.uvsMin                 = rtGeometry.uvsMin;
				rtInstance.uvsRange               = rtGeometry.uvsRange;

				rtInstances[instanceIdx] = rtInstance;

				mat::RTHitGroupPermutation hitGroupPermutation;
				hitGroupPermutation.SHADER = materialProxy.params.shader;
				const Uint32 hitGroupIdx = mat::MaterialsSubsystem::Get().GetRTHitGroupIdx(hitGroupPermutation);

				ETLASGeometryMask mask = ETLASGeometryMask::Opaque;
				if (materialProxy.params.transparent)
				{
					mask = ETLASGeometryMask::Transparent;
				}

				rhi::TLASInstanceDefinition tlasInstance;
				tlasInstance.transform       = transformMatrix;
				tlasInstance.blasAddress     = rtGeometry.blas->GetRHI().GetDeviceAddress();
				tlasInstance.customIdx       = instanceIdx;
				tlasInstance.sbtRecordOffset = hitGroupIdx;
				tlasInstance.mask            = static_cast<Uint32>(mask);

				rhi::RHIASUtils::CopyInstanceDefinitionToBuffer(rtInstancesDefs, instanceIdx, tlasInstance);

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

	const Uint32 instancesNum = currentInstanceIdx;

	if (instancesNum > 0)
	{
		const rg::RGBufferViewHandle scratchBuffer = graphBuilder.CreateStorageBufferView(RG_DEBUG_NAME("TLAS Builder Scratch Buffer"), m_tlas->GetRHI().GetBuildScratchSize(), rhi::EMemoryUsage::GPUOnly);

		graphBuilder.BuildTLAS(RG_DEBUG_NAME("Build Scene TLAS"), 
				rg::TLASBuildCommand
				{
					.tlas                   = m_tlas,
					.instanceDefsBufferView = graphBuilder.AcquireExternalBufferView(m_instancesDefsBuffer->GetFullView()),
					.instancesNum           = instancesNum,
					.scratchBufferView      = scratchBuffer
				});

		const rg::RGBufferViewHandle rtInstancesBuffer = graphBuilder.AcquireExternalBufferView(m_rtInstancesDataBuffer->GetFullView());
		const rg::RGBufferViewHandle rtInstancesStagingBuffer = graphBuilder.AcquireExternalBufferView(m_rtInstancesDataStagingBuffer->GetFullView());
		graphBuilder.CopyBuffer(RG_DEBUG_NAME("Upload RT Instances Data"),
								rtInstancesStagingBuffer, 0,
								rtInstancesBuffer, 0,
								instancesNum * sizeof(rdr::HLSLStorage<RTInstanceData>));
	}
}

} // spt::rsc
