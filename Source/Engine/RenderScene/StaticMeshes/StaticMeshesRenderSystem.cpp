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
		lib::DynamicArray<StaticMeshBatchDefinition> batchDefinitions = staticMeshPrimsSystem.BuildBatchesForView(viewSpec.GetRenderView(), mat::EMaterialType::Opaque);
		
		if (!batchDefinitions.empty())
		{
			if (supportsDepthPrepass)
			{
				const Bool hasAnyBatches = m_depthPrepassRenderer.BuildBatchesPerView(graphBuilder, renderScene, viewSpec, batchDefinitions);

				if (hasAnyBatches)
				{
					m_depthPrepassRenderer.CullPerView(graphBuilder, renderScene, viewSpec);

					RenderStageEntries& depthPrepassStageEntries = viewSpec.GetRenderStageEntries(ERenderStage::DepthPrepass);
					depthPrepassStageEntries.GetOnRenderStage().AddRawMember(&m_depthPrepassRenderer, &StaticMeshDepthPrepassRenderer::RenderPerView);
				}
			}

			if (supportsForwardOpaque)
			{
				const Bool hasAnyBatches = m_forwardOpaqueRenderer.BuildBatchesPerView(graphBuilder, renderScene, viewSpec, batchDefinitions);

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

} // spt::rsc
