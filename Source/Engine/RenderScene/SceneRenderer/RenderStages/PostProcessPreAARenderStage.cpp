#include "PostProcessPreAARenderStage.h"
#include "Utils/DOFRenderer.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"

namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::PostProcessPreAA, PostProcessPreAARenderStage);

namespace params
{

RendererBoolParameter enableDOF("Enable DOF", { "DOF" }, false);
RendererFloatParameter focalPlane("Focal Plane", { "DOF" }, 4.f, 0.f, 20.f);
RendererFloatParameter fullFocusRange("Full Focus Range", { "DOF" }, 1.f, 0.f, 10.f);
RendererFloatParameter farFocusIncreaseRange("Far Focus Increase Range", { "DOF" }, 7.f, 0.f, 10.f);
RendererFloatParameter nearFocusIncreaseRange("Near Focus Increase Range", { "DOF" }, 2.f, 0.f, 10.f);

} // params

PostProcessPreAARenderStage::PostProcessPreAARenderStage()
{ }

void PostProcessPreAARenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();
	
	if (params::enableDOF)
	{
		const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

		dof::GatherBasedDOFParameters dofParameters;
		dofParameters.linearColorTexture		= viewContext.luminance;
		dofParameters.depthTexture				= viewContext.depth;
		dofParameters.viewDS					= viewSpec.GetRenderView().GetRenderViewDS();
		dofParameters.focalPlane				= params::focalPlane;
		dofParameters.fullFocusRange			= params::fullFocusRange;
		dofParameters.farFocusIncreaseRange		= params::farFocusIncreaseRange;
		dofParameters.nearFocusIncreaseRange	= params::nearFocusIncreaseRange;
		dof::RenderGatherBasedDOF(graphBuilder, dofParameters);
	}

	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
