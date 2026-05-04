#include "DirectionalLightShadowMasksRenderStage.h"
#include "RenderScene.h"
#include "Lights/LightTypes.h"
#include "SceneRenderSystems/Lights/ViewShadingInput.h"


namespace spt::rsc
{
REGISTER_RENDER_STAGE(ERenderStage::DirectionalLightsShadowMasks, DirectionalLightShadowMasksRenderStage);


DirectionalLightShadowMasksRenderStage::DirectionalLightShadowMasksRenderStage()
{ }

void DirectionalLightShadowMasksRenderStage::BeginFrame(const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	Base::BeginFrame(renderScene, viewSpec);

	m_shadowMaskRenderer.BeginFrame(renderScene, viewSpec);
}

void DirectionalLightShadowMasksRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(rdr::GPUApi::IsRayTracingEnabled());

	ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	viewSpec.GetRenderViewEntry(ERenderViewEntry::RenderCloudsTransmittanceMap).Broadcast(graphBuilder, rendererInterface, renderScene, viewSpec, RenderViewEntryContext{});

	viewContext.dirLightShadowMask = m_shadowMaskRenderer.Render(graphBuilder, rendererInterface, renderScene, viewSpec);
}

} // spt::rsc
