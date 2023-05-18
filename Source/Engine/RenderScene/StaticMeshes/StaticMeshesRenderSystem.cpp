#include "StaticMeshesRenderSystem.h"
#include "RenderScene.h"
#include "StaticMeshRenderSceneSubsystem.h"
#include "Transfers/UploadUtils.h"

namespace spt::rsc
{

StaticMeshesRenderSystem::StaticMeshesRenderSystem()
{
	m_supportedStages = lib::Flags(ERenderStage::ForwardOpaque, ERenderStage::DepthPrepass, ERenderStage::ShadowMap);
}

void StaticMeshesRenderSystem::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene)
{
	Super::RenderPerFrame(graphBuilder, renderScene);

	m_shadowMapRenderer.RenderPerFrame(graphBuilder, renderScene);
}

void StaticMeshesRenderSystem::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	Super::RenderPerView(graphBuilder, renderScene, viewSpec);
	
	if (viewSpec.SupportsStage(ERenderStage::ShadowMap))
	{
		RenderStageEntries& shadowMapStageEntries = viewSpec.GetRenderStageEntries(ERenderStage::ShadowMap);
		shadowMapStageEntries.GetOnRenderStage().AddRawMember(&m_shadowMapRenderer, &StaticMeshShadowMapRenderer::RenderPerView);
	}

	const Bool supportsDepthPrepass		= viewSpec.SupportsStage(ERenderStage::DepthPrepass);
	const Bool supportsForwardOpaque	= viewSpec.SupportsStage(ERenderStage::ForwardOpaque);

	if (supportsDepthPrepass || supportsForwardOpaque)
	{
		const StaticMeshRenderSceneSubsystem& staticMeshPrimsSystem = renderScene.GetSceneSubsystemChecked<StaticMeshRenderSceneSubsystem>();
		const StaticMeshBatchDefinition batchDefinition = staticMeshPrimsSystem.BuildBatchForView(viewSpec.GetRenderView(), EMaterialType::Opaque);
		
		if (batchDefinition.IsValid())
		{
			const lib::SharedRef<StaticMeshBatchDS> batchDS = CreateBatchDS(batchDefinition);

			if (supportsDepthPrepass)
			{
				const Bool hasAnyBatches = m_depthPrepassRenderer.BuildBatchesPerView(graphBuilder, renderScene, viewSpec, batchDefinition, batchDS);

				if (hasAnyBatches)
				{
					m_depthPrepassRenderer.CullPerView(graphBuilder, renderScene, viewSpec);

					RenderStageEntries& depthPrepassStageEntries = viewSpec.GetRenderStageEntries(ERenderStage::DepthPrepass);
					depthPrepassStageEntries.GetOnRenderStage().AddRawMember(&m_depthPrepassRenderer, &StaticMeshDepthPrepassRenderer::RenderPerView);
				}
			}

			if (supportsForwardOpaque)
			{
				const Bool hasAnyBatches = m_forwardOpaqueRenderer.BuildBatchesPerView(graphBuilder, renderScene, viewSpec, batchDefinition, batchDS);

				if (hasAnyBatches)
				{
					RenderStageEntries& motionAndDepthStageEntries = viewSpec.GetRenderStageEntries(ERenderStage::MotionAndDepth);
					motionAndDepthStageEntries.GetPostRenderStage().AddRawMember(&m_forwardOpaqueRenderer, &StaticMeshForwardOpaqueRenderer::CullPerView);

					RenderStageEntries& basePassStageEntries = viewSpec.GetRenderStageEntries(ERenderStage::ForwardOpaque);
					basePassStageEntries.GetOnRenderStage().AddRawMember(&m_forwardOpaqueRenderer, &StaticMeshForwardOpaqueRenderer::RenderPerView);
				}
			}
		}
	}
}

lib::SharedRef<StaticMeshBatchDS> StaticMeshesRenderSystem::CreateBatchDS(const StaticMeshBatchDefinition& batchDef) const
{
	SPT_PROFILER_FUNCTION();

	// Create GPU batch data

	SMGPUBatchData gpuBatchData;
	gpuBatchData.elementsNum = static_cast<Uint32>(batchDef.batchElements.size());

	// Create Batch elements buffer

	const Uint64 batchDataSize = sizeof(StaticMeshBatchElement) * batchDef.batchElements.size();
	const rhi::BufferDefinition batchBufferDef(batchDataSize, rhi::EBufferUsage::Storage);
	const rhi::RHIAllocationInfo batchBufferAllocation(rhi::EMemoryUsage::CPUToGPU);
	const lib::SharedRef<rdr::Buffer> batchBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("StaticMeshesBatch"), batchBufferDef, batchBufferAllocation);

	gfx::UploadDataToBuffer(batchBuffer, 0, reinterpret_cast<const Byte*>(batchDef.batchElements.data()), batchDataSize);

	const lib::SharedRef<StaticMeshBatchDS> batchDS = rdr::ResourcesManager::CreateDescriptorSetState<StaticMeshBatchDS>(RENDERER_RESOURCE_NAME("StaticMeshesBatchDS"));
	batchDS->u_batchElements	= batchBuffer->CreateFullView();
	batchDS->u_batchData		= gpuBatchData;

	return batchDS;
}

} // spt::rsc
