#include "PostProcessPreAARenderStage.h"
#include "Utils/DOFRenderer.h"

namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::PostProcessPreAA, PostProcessPreAARenderStage);

PostProcessPreAARenderStage::PostProcessPreAARenderStage()
{ }

void PostProcessPreAARenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	const DOFSettings& dofSettings = viewSpec.GetSettings().dof;

	if (dofSettings.enable)
	{
		const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

		dof::GatherBasedDOFParameters dofParameters;
		dofParameters.linearColorTexture		= viewContext.luminance;
		dofParameters.depthTexture				= viewContext.depth;
		dofParameters.focalPlane				= dofSettings.focalPlane;
		dofParameters.fullFocusRange			= dofSettings.fullFocusRange;
		dofParameters.farFocusIncreaseRange		= dofSettings.farFocusIncreaseRange;
		dofParameters.nearFocusIncreaseRange	= dofSettings.nearFocusIncreaseRange;
		dof::RenderGatherBasedDOF(graphBuilder, dofParameters);
	}
}

} // spt::rsc
