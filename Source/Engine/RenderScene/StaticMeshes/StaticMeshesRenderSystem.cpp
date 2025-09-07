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

void StaticMeshesRenderSystem::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings)
{
	Super::RenderPerFrame(graphBuilder, renderScene, viewSpecs, settings);

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
}

} // spt::rsc
