#include "ForwardOpaqueRenderStage.h"
#include "View/ViewRenderingSpec.h"
#include "SceneRenderer/SceneRendererTypes.h"
#include "View/RenderView.h"
#include "RenderGraphBuilder.h"
#include "DepthPrepassRenderStage.h"
#include "Lights/ViewShadingInput.h"
#include "Shadows/ShadowMapsManagerSystem.h"

namespace spt::rsc
{

RenderTargetFormatsDef ForwardOpaqueRenderStage::GetRenderTargetFormats()
{
	RenderTargetFormatsDef formats;
	formats.colorRTFormats.emplace_back(rhi::EFragmentFormat::B10G11B11_U_Float);
	formats.colorRTFormats.emplace_back(rhi::EFragmentFormat::RGBA16_UN_Float);
#if RENDERER_DEBUG
	formats.colorRTFormats.emplace_back(rhi::EFragmentFormat::RGBA8_UN_Float);
#endif // RENDERER_DEBUG
	formats.depthRTFormat = DepthPrepassRenderStage::GetDepthFormat();

	return formats;
}

ForwardOpaqueRenderStage::ForwardOpaqueRenderStage()
{ }

void ForwardOpaqueRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	const DepthPrepassData& depthPrepassData	= viewSpec.GetData().Get<DepthPrepassData>();
	ShadingData& passData						= viewSpec.GetData().Create<ShadingData>();

	SPT_CHECK(depthPrepassData.depth.IsValid());

	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingResolution();
	const math::Vector3u texturesRes(renderingRes.x(), renderingRes.y(), 1);

	const rhi::ETextureUsage passTexturesUsage = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::ColorRT);
	
	rhi::TextureDefinition radianceDef;
	radianceDef.resolution	= texturesRes;
	radianceDef.usage		= lib::Flags(passTexturesUsage, rhi::ETextureUsage::TransferSource, rhi::ETextureUsage::TransferDest);
	radianceDef.format		= rhi::EFragmentFormat::B10G11B11_U_Float;
	passData.radiance = graphBuilder.CreateTextureView(RG_DEBUG_NAME("ViewRadianceTexture"), radianceDef, rhi::EMemoryUsage::GPUOnly);
	
	rhi::TextureDefinition normalsDef;
	normalsDef.resolution	= texturesRes;
	normalsDef.usage		= passTexturesUsage;
	normalsDef.format		= rhi::EFragmentFormat::RGBA16_UN_Float;
	passData.normals = graphBuilder.CreateTextureView(RG_DEBUG_NAME("ViewNormalsTexture"), normalsDef, rhi::EMemoryUsage::GPUOnly);

#if RENDERER_DEBUG
	rhi::TextureDefinition debugDef;
	debugDef.resolution	= texturesRes;
	debugDef.usage		= passTexturesUsage;
	debugDef.format		= rhi::EFragmentFormat::RGBA8_UN_Float;
	passData.debug = graphBuilder.CreateTextureView(RG_DEBUG_NAME("DebugTexture"), debugDef, rhi::EMemoryUsage::GPUOnly);
#endif // RENDERER_DEBUG

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i(0, 0), renderingRes);

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView		= depthPrepassData.depth;
	depthRTDef.loadOperation	= rhi::ERTLoadOperation::Load;
	depthRTDef.storeOperation	= rhi::ERTStoreOperation::DontCare;
	depthRTDef.clearColor		= rhi::ClearColor(0.f);
	renderPassDef.SetDepthRenderTarget(depthRTDef);

	rg::RGRenderTargetDef radianceRTDef;
	radianceRTDef.textureView		= passData.radiance;
	radianceRTDef.loadOperation		= rhi::ERTLoadOperation::Clear;
	radianceRTDef.storeOperation	= rhi::ERTStoreOperation::Store;
	radianceRTDef.clearColor		= rhi::ClearColor(0.f, 0.f, 0.f, 1.f);
	renderPassDef.AddColorRenderTarget(radianceRTDef);

	rg::RGRenderTargetDef normalsRTDef;
	normalsRTDef.textureView	= passData.normals;
	normalsRTDef.loadOperation	= rhi::ERTLoadOperation::Clear;
	normalsRTDef.storeOperation	= rhi::ERTStoreOperation::Store;
	normalsRTDef.clearColor		= rhi::ClearColor(0.f, 0.f, 0.f, 0.f);
	renderPassDef.AddColorRenderTarget(normalsRTDef);

#if RENDERER_DEBUG
	rg::RGRenderTargetDef debugRTDef;
	debugRTDef.textureView		= passData.debug;
	debugRTDef.loadOperation	= rhi::ERTLoadOperation::Clear;
	debugRTDef.storeOperation	= rhi::ERTStoreOperation::Store;
	debugRTDef.clearColor		= rhi::ClearColor(0.f, 0.f, 0.f, 0.f);
	renderPassDef.AddColorRenderTarget(debugRTDef);
#endif // RENDERER_DEBUG
	
	const math::Vector2u renderingArea = viewSpec.GetRenderView().GetRenderingResolution();

	const ViewSpecShadingData& shadingData = viewSpec.GetData().Get<ViewSpecShadingData>();

	graphBuilder.RenderPass(RG_DEBUG_NAME("Forward Opaque Render Pass"),
							renderPassDef,
							rg::BindDescriptorSets(lib::Ref(shadingData.shadingInputDS), lib::Ref(shadingData.shadowMapsDS)),
							[renderingArea](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), renderingArea.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), renderingArea));
							});

	GetStageEntries(viewSpec).GetOnRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
