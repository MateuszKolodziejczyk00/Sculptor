#include "RayTracingRenderSceneSubsystem.h"
#include "RenderScene.h"
#include "RayTracingSceneTypes.h"
#include "Types/AccelerationStructure.h"
#include "ResourcesManager.h"
#include "Renderer.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Types/RenderContext.h"

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

void RayTracingRenderSceneSubsystem::UpdateTLAS()
{
	SPT_PROFILER_FUNCTION();

	const RenderSceneRegistry& sceneRegistry = GetOwningScene().GetRegistry();

	rhi::TLASDefinition tlasDefinition;

	const auto rayTracedObjectsEntities = sceneRegistry.view<TransformComponent, BLASesProviderComponent>();
	const SizeType rayTracedEntitiesNum = static_cast<SizeType>(rayTracedObjectsEntities.size_hint() * 2.2f);
	tlasDefinition.instances.reserve(rayTracedEntitiesNum);

	for (const auto& [entity, transform, blasesProvider] : rayTracedObjectsEntities.each())
	{
		const BLASesComponent& blasesComp = blasesProvider.entity.get<BLASesComponent>();

		const rhi::TLASInstanceDefinition::TransformMatrix transformMatrix = transform.GetTransform().matrix().topLeftCorner<3, 4>();

		for (const lib::SharedPtr<rdr::BottomLevelAS>& blas : blasesComp.blases)
		{
			rhi::TLASInstanceDefinition& tlasInstance = tlasDefinition.instances.emplace_back();
			tlasInstance.transform		= transformMatrix;
			tlasInstance.blasAddress	= blas->GetRHI().GetDeviceAddress();
		}
	}

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

} // spt::rsc
