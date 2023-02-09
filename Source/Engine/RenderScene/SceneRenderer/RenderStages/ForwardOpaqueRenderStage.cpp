#include "ForwardOpaqueRenderStage.h"
#include "View/ViewRenderingSpec.h"
#include "SceneRenderer/SceneRendererTypes.h"
#include "View/RenderView.h"
#include "RenderGraphBuilder.h"
#include "DepthPrepassRenderStage.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ResourcesManager.h"
#include "Common/ShaderCompilationInput.h"

namespace spt::rsc
{

namespace tonemapping
{

BEGIN_SHADER_STRUCT(TonemappingSettings)
	SHADER_STRUCT_FIELD(math::Vector2u, textureSize)
	SHADER_STRUCT_FIELD(math::Vector2f, invTextureSize)
END_SHADER_STRUCT();


DS_BEGIN(TonemappingDS, rg::RGDescriptorSetState<TonemappingDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>), u_radianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToBorder>), u_sampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>), u_LDRTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<TonemappingSettings>), u_tonemappingSettings)
DS_END();


rdr::PipelineStateID CompileTonemappingPipeline()
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "TonemappingCS"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/Tonemapping.hlsl", compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("TonemappingPipeline"), shader);
}


rg::RGTextureViewHandle DoTonemappingAndGammaCorrection(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, rg::RGTextureViewHandle radianceTexture)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingResolution();
	const math::Vector3u textureRes(renderingRes.x(), renderingRes.y(), 1);
	
	rhi::TextureDefinition tonemappedTextureDef;
	tonemappedTextureDef.resolution = textureRes;
	tonemappedTextureDef.usage		= lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferSource);
	tonemappedTextureDef.format		= rhi::EFragmentFormat::RGBA8_UN_Float;
	const rg::RGTextureViewHandle tonemappedTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("TonemappedTexture"), tonemappedTextureDef, rhi::EMemoryUsage::GPUOnly);

	TonemappingSettings tonemappingSettings;
	tonemappingSettings.textureSize		= renderingRes;
	tonemappingSettings.invTextureSize	= math::Vector2f(1.f / static_cast<Real32>(renderingRes.x()), 1.f / static_cast<Real32>(renderingRes.y()));

	const lib::SharedRef<TonemappingDS> tonemappingDS = rdr::ResourcesManager::CreateDescriptorSetState<TonemappingDS>(RENDERER_RESOURCE_NAME("TonemappingDS"));
	tonemappingDS->u_radianceTexture.Set(radianceTexture);
	tonemappingDS->u_LDRTexture.Set(tonemappedTexture);
	tonemappingDS->u_tonemappingSettings = tonemappingSettings;

	static const rdr::PipelineStateID pipelineState = CompileTonemappingPipeline();

	const math::Vector3u dispatchGroups(math::Utils::DivideCeil(textureRes.x(), 8u), math::Utils::DivideCeil(textureRes.y(), 8u), 1);
	graphBuilder.Dispatch(RG_DEBUG_NAME("TonemappingAndGamma"), pipelineState, dispatchGroups, rg::BindDescriptorSets(tonemappingDS));

	return tonemappedTexture;
}

} // tonemapping

RenderTargetFormatsDef ForwardOpaqueRenderStage::GetRenderTargetFormats()
{
	RenderTargetFormatsDef formats;
	formats.colorRTFormats.emplace_back(rhi::EFragmentFormat::B10G11B11_U_Float);
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
	ForwardOpaquePassResult& passData				= viewSpec.GetData().Create<ForwardOpaquePassResult>();

	SPT_CHECK(depthPrepassData.depth.IsValid());

	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingResolution();
	const math::Vector3u texturesRes(renderingRes.x(), renderingRes.y(), 1);

	const rhi::ETextureUsage passTexturesUsage = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::ColorRT);
	
	rhi::TextureDefinition radianceDef;
	radianceDef.resolution	= texturesRes;
	radianceDef.usage		= passTexturesUsage;
	radianceDef.format		= rhi::EFragmentFormat::B10G11B11_U_Float;
	passData.radiance = graphBuilder.CreateTextureView(RG_DEBUG_NAME("ViewRadianceTexture"), radianceDef, rhi::EMemoryUsage::GPUOnly);
	
	rhi::TextureDefinition normalsDef;
	normalsDef.resolution	= texturesRes;
	normalsDef.usage		= passTexturesUsage;
	normalsDef.format		= rhi::EFragmentFormat::RGBA16_UN_Float;
	passData.normals = graphBuilder.CreateTextureView(RG_DEBUG_NAME("ViewNormalsTexture"), normalsDef, rhi::EMemoryUsage::GPUOnly);

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i(0, 0), renderingRes);

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView						= depthPrepassData.depth;
	depthRTDef.loadOperation					= rhi::ERTLoadOperation::Load;
	depthRTDef.storeOperation					= rhi::ERTStoreOperation::Store;
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
	
	const math::Vector2u renderingArea = viewSpec.GetRenderView().GetRenderingResolution();

	graphBuilder.RenderPass(RG_DEBUG_NAME("Forward Opaque Render Pass"),
							renderPassDef,
							rg::EmptyDescriptorSets(),
							[renderingArea](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), renderingArea.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), renderingArea));
							});

	GetStageEntries(viewSpec).GetOnRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);

	passData.tonamappedTexture = tonemapping::DoTonemappingAndGammaCorrection(graphBuilder, viewSpec, passData.radiance);
}

} // spt::rsc
