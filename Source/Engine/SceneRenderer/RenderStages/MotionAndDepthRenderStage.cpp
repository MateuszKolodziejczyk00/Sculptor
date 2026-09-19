#include "MotionAndDepthRenderStage.h"
#include "View/RenderView.h"
#include "RenderGraphBuilder.h"
#include "ResourcesManager.h"
#include "Common/ShaderCompilationInput.h"
#include "Utils/ViewRenderingSpec.h"
#include "Utils/hiZRenderer.h"


namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::MotionAndDepth, MotionAndDepthRenderStage);

namespace camera_motion
{

BEGIN_SHADER_STRUCT(CameraMotionConstants)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,               depth)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector2f>,       motion)
END_SHADER_STRUCT();


static rdr::PipelineStateID CompileCameraMotionPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/MotionAndDepth/CameraMotion.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CameraMotionCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("CameraMotionPipeline"), shader);
}

void ComputeCameraMotion(rg::RenderGraphBuilder& graphBuilder, const ViewRenderingSpec& viewSpec, const rg::RGTextureViewHandle motionTextureView, const rg::RGTextureViewHandle depthTextureView)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(motionTextureView.IsValid());
	SPT_CHECK(depthTextureView.IsValid());

	const math::Vector2u renderingRes = viewSpec.GetRenderingRes();

	static const rdr::PipelineStateID pipeline = CompileCameraMotionPipeline();

	CameraMotionConstants shaderConstants;
	shaderConstants.depth	= depthTextureView;
	shaderConstants.motion	= motionTextureView;

	const math::Vector2u dispatchGroups = math::Utils::DivideCeil(renderingRes, math::Vector2u(8, 8));

	graphBuilder.Dispatch(RG_DEBUG_NAME("Camera Motion"),
						  pipeline,
						  math::Vector3u(dispatchGroups.x(), dispatchGroups.y(), 1u),
						  rg::ShaderParams(shaderConstants));
}

} // camera_motion

rhi::EFragmentFormat MotionAndDepthRenderStage::GetMotionFormat()
{
	return rhi::EFragmentFormat::RG16_SN_Float;
}

MotionAndDepthRenderStage::MotionAndDepthRenderStage()
{ }

void MotionAndDepthRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();
	
	const math::Vector2u renderingRes = viewSpec.GetRenderingRes();

	ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	viewContext.motion = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Motion Texture"), rg::TextureDef(renderingRes, GetMotionFormat()));

	// Render camera only motion as default
	camera_motion::ComputeCameraMotion(graphBuilder, viewSpec, viewContext.motion, viewContext.depth);
}

} // spt::rsc
