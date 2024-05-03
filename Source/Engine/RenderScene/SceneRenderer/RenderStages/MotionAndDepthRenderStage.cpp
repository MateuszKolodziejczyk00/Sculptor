#include "MotionAndDepthRenderStage.h"
#include "View/RenderView.h"
#include "RenderGraphBuilder.h"
#include "ResourcesManager.h"
#include "Common/ShaderCompilationInput.h"
#include "View/ViewRenderingSpec.h"
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

void ComputeCameraMotion(rg::RenderGraphBuilder& graphBuilder, const RenderView& renderView, const rg::RGTextureViewHandle motionTextureView, const rg::RGTextureViewHandle depthTextureView)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(motionTextureView.IsValid());
	SPT_CHECK(depthTextureView.IsValid());

	const math::Vector2u renderingRes = renderView.GetRenderingRes();

	static const rdr::PipelineStateID pipeline = CompileCameraMotionPipeline();

	const lib::MTHandle<CameraMotionDS> cameraMotionDS = graphBuilder.CreateDescriptorSet<CameraMotionDS>(RENDERER_RESOURCE_NAME("CameraMotionDS"));
	cameraMotionDS->u_depth		= depthTextureView;
	cameraMotionDS->u_motion	= motionTextureView;

	const math::Vector2u dispatchGroups = math::Utils::DivideCeil(renderingRes, math::Vector2u(8, 8));

	graphBuilder.Dispatch(RG_DEBUG_NAME("Camera Motion"),
						  pipeline,
						  math::Vector3u(dispatchGroups.x(), dispatchGroups.y(), 1u),
						  rg::BindDescriptorSets(renderView.GetRenderViewDS(), cameraMotionDS));
}

} // camera_motion

rhi::EFragmentFormat MotionAndDepthRenderStage::GetMotionFormat()
{
	return rhi::EFragmentFormat::RG16_SN_Float;
}

MotionAndDepthRenderStage::MotionAndDepthRenderStage()
{ }

void MotionAndDepthRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();
	
	const math::Vector2u renderingRes = renderView.GetRenderingRes();
	const math::Vector2u halfRes      = renderView.GetRenderingHalfRes();

	ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	viewContext.motion        = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Motion Texture"), rg::TextureDef(renderingRes, GetMotionFormat()));
	viewContext.motionHalfRes = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Motion Texture Half Res"), rg::TextureDef(halfRes, GetMotionFormat()));

	// Render camera only motion as default
	camera_motion::ComputeCameraMotion(graphBuilder, renderView, viewContext.motion, viewContext.depth);

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i(0, 0), renderingRes);

	rg::RGRenderTargetDef velocityRTDef;
	velocityRTDef.textureView		= viewContext.motion;
	velocityRTDef.loadOperation		= rhi::ERTLoadOperation::Load;
	velocityRTDef.storeOperation	= rhi::ERTStoreOperation::Store;
	velocityRTDef.clearColor		= rhi::ClearColor(0.f, 0.f, 0.f, 0.f);
	renderPassDef.AddColorRenderTarget(velocityRTDef);

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView		= viewContext.depth;
	depthRTDef.loadOperation	= rhi::ERTLoadOperation::Load;
	depthRTDef.storeOperation	= rhi::ERTStoreOperation::Store;
	renderPassDef.SetDepthRenderTarget(depthRTDef);

	const math::Vector2u renderingArea = viewSpec.GetRenderView().GetRenderingRes();
	
	// Render pass to render dynamic objects with custom velocity calculations
	graphBuilder.RenderPass(RG_DEBUG_NAME("Velocity And Depth Pass"),
							renderPassDef,
							rg::EmptyDescriptorSets(),
							[renderingArea](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), renderingArea.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), renderingArea));
							});

	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
