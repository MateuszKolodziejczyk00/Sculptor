#include "ForwardOpaqueRenderStage.h"
#include "View/ViewRenderingSpec.h"
#include "SceneRenderer/SceneRendererTypes.h"
#include "View/RenderView.h"
#include "RenderGraphBuilder.h"
#include "DepthPrepassRenderStage.h"

namespace spt::rsc
{

RenderTargetFormatsDef ForwardOpaqueRenderStage::GetRenderTargetFormats()
{
	RenderTargetFormatsDef formats;
	formats.colorRTFormats.emplace_back(rhi::EFragmentFormat::RGBA8_UN_Float);
	formats.colorRTFormats.emplace_back(rhi::EFragmentFormat::RGBA16_UN_Float);
	formats.depthRTFormat = DepthPrepassRenderStage::GetDepthFormat();

	return formats;
}

ForwardOpaqueRenderStage::ForwardOpaqueRenderStage()
{ }

void ForwardOpaqueRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	const DepthPrepassResult& depthPrepassData	= viewSpec.GetData().Get<DepthPrepassResult>();
	ForwardOpaquePassResult& foData				= viewSpec.GetData().Create<ForwardOpaquePassResult>();

	SPT_CHECK(depthPrepassData.depth.IsValid());

	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingResolution();
	const math::Vector3u texturesRes(renderingRes.x(), renderingRes.y(), 1);

	const rhi::RHIAllocationInfo ofTextureAllocationInfo(rhi::EMemoryUsage::GPUOnly);
	const rhi::ETextureUsage ofTexturesUsage = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::ColorRT);
	
	rhi::TextureDefinition radianceDef;
	radianceDef.resolution = texturesRes;
	radianceDef.usage = lib::Flags(ofTexturesUsage, rhi::ETextureUsage::TransferSource);
	radianceDef.format = rhi::EFragmentFormat::RGBA8_UN_Float;
	foData.radiance = graphBuilder.CreateTextureView(RG_DEBUG_NAME("ViewRadianceTexture"), radianceDef, ofTextureAllocationInfo);
	
	rhi::TextureDefinition normalsDef;
	normalsDef.resolution = texturesRes;
	normalsDef.usage = ofTexturesUsage;
	normalsDef.format = rhi::EFragmentFormat::RGBA16_UN_Float;
	foData.normals = graphBuilder.CreateTextureView(RG_DEBUG_NAME("ViewNormalsTexture"), normalsDef, ofTextureAllocationInfo);

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i(0, 0), renderingRes);

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView						= depthPrepassData.depth;
	depthRTDef.loadOperation					= rhi::ERTLoadOperation::Load;
	depthRTDef.storeOperation					= rhi::ERTStoreOperation::Store;
	renderPassDef.SetDepthRenderTarget(depthRTDef);

	rg::RGRenderTargetDef radianceRTDef;
	radianceRTDef.textureView		= foData.radiance;
	radianceRTDef.loadOperation		= rhi::ERTLoadOperation::Clear;
	radianceRTDef.storeOperation	= rhi::ERTStoreOperation::Store;
	radianceRTDef.clearColor		= rhi::ClearColor(0.f, 0.f, 0.f, 1.f);
	renderPassDef.AddColorRenderTarget(radianceRTDef);

	rg::RGRenderTargetDef normalsRTDef;
	normalsRTDef.textureView	= foData.normals;
	normalsRTDef.loadOperation	= rhi::ERTLoadOperation::Clear;
	normalsRTDef.storeOperation	= rhi::ERTStoreOperation::Store;
	normalsRTDef.clearColor		= rhi::ClearColor(0.f, 0.f, 0.f, 0.f);
	renderPassDef.AddColorRenderTarget(normalsRTDef);

	graphBuilder.RenderPass(RG_DEBUG_NAME("Forward Opaque Render Pass"),
							renderPassDef,
							rg::EmptyDescriptorSets(),
							[](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder) {});

	GetStageEntries(viewSpec).GetOnRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
