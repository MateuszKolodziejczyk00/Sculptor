#include "MotionAndDepthRenderStage.h"
#include "View/RenderView.h"
#include "RenderGraphBuilder.h"
#include "ResourcesManager.h"
#include "Common/ShaderCompilationInput.h"
#include "Utils/ViewRenderingSpec.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "Utils/hiZRenderer.h"


namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::MotionAndDepth, MotionAndDepthRenderStage);

namespace camera_motion
{

DS_BEGIN(CameraMotionDS, rg::RGDescriptorSetState<CameraMotionDS>)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToBorder>),	u_depthSampler)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_depth)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector2f>),								u_motion)
DS_END();


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

	const lib::MTHandle<CameraMotionDS> cameraMotionDS = graphBuilder.CreateDescriptorSet<CameraMotionDS>(RENDERER_RESOURCE_NAME("CameraMotionDS"));
	cameraMotionDS->u_depth		= depthTextureView;
	cameraMotionDS->u_motion	= motionTextureView;

	const math::Vector2u dispatchGroups = math::Utils::DivideCeil(renderingRes, math::Vector2u(8, 8));

	graphBuilder.Dispatch(RG_DEBUG_NAME("Camera Motion"),
						  pipeline,
						  math::Vector3u(dispatchGroups.x(), dispatchGroups.y(), 1u),
						  rg::BindDescriptorSets(cameraMotionDS));
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
