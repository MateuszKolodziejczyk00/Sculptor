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

void StaticMeshesRenderSystem::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs)
{
	Super::RenderPerFrame(graphBuilder, renderScene, viewSpecs);

	{
		lib::DynamicArray<ViewRenderingSpec*> shadowMapViewSpecs;
		std::copy_if(std::cbegin(viewSpecs), std::cend(viewSpecs),
					 std::back_inserter(shadowMapViewSpecs),
					 [](const ViewRenderingSpec* viewSpec)
					 {
						 return viewSpec->SupportsStage(ERenderStage::ShadowMap);
					 });

		m_shadowMapRenderer.RenderPerFrame(graphBuilder, renderScene, shadowMapViewSpecs);
	}

	for (ViewRenderingSpec* viewSpec : viewSpecs)
	{
		SPT_CHECK(!!viewSpec);
		RenderPerView(graphBuilder, renderScene, *viewSpec);
	}
}

void StaticMeshesRenderSystem::FinishRenderingFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene)
{
	Super::FinishRenderingFrame(graphBuilder, renderScene);

	m_shadowMapRenderer.FinishRenderingFrame();
}

void StaticMeshesRenderSystem::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();
	
	if (viewSpec.SupportsStage(ERenderStage::ShadowMap))
	{
		RenderStageEntries& shadowMapStageEntries = viewSpec.GetRenderStageEntries(ERenderStage::ShadowMap);
		shadowMapStageEntries.GetOnRenderStage().AddRawMember(&m_shadowMapRenderer, &StaticMeshShadowMapRenderer::RenderToShadowMap);
	}

	const Bool supportsDepthPrepass		= viewSpec.SupportsStage(ERenderStage::DepthPrepass);
	const Bool supportsForwardOpaque	= viewSpec.SupportsStage(ERenderStage::ForwardOpaque);

	if (supportsDepthPrepass || supportsForwardOpaque)
	{
		const StaticMeshRenderSceneSubsystem& staticMeshPrimsSystem = renderScene.GetSceneSubsystemChecked<StaticMeshRenderSceneSubsystem>();
		const lib::DynamicArray<StaticMeshBatchDefinition>& batchDefinitions = staticMeshPrimsSystem.BuildBatchesForView(viewSpec.GetRenderView());
		
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
