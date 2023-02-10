#include "HDRResolveRenderStage.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ResourcesManager.h"
#include "Common/ShaderCompilationInput.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "SceneRenderer/SceneRendererTypes.h"
#include "View/ViewRenderingSpec.h"
#include "View/RenderView.h"

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


void DoTonemappingAndGammaCorrection(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, rg::RGTextureViewHandle radianceTexture, rg::RGTextureViewHandle ldrTexture, const TonemappingSettings& tonemappingSettings)
{
	SPT_PROFILER_FUNCTION();

	const lib::SharedRef<TonemappingDS> tonemappingDS = rdr::ResourcesManager::CreateDescriptorSetState<TonemappingDS>(RENDERER_RESOURCE_NAME("TonemappingDS"));
	tonemappingDS->u_radianceTexture		= radianceTexture;
	tonemappingDS->u_LDRTexture				= ldrTexture;
	tonemappingDS->u_tonemappingSettings	= tonemappingSettings;

	static const rdr::PipelineStateID pipelineState = CompileTonemappingPipeline();

	const math::Vector2u textureRes = tonemappingSettings.textureSize;
	const math::Vector3u dispatchGroups(math::Utils::DivideCeil(textureRes.x(), 8u), math::Utils::DivideCeil(textureRes.y(), 8u), 1);
	graphBuilder.Dispatch(RG_DEBUG_NAME("TonemappingAndGamma"), pipelineState, dispatchGroups, rg::BindDescriptorSets(tonemappingDS));
}

} // tonemapping

HDRResolveRenderStage::HDRResolveRenderStage()
{ }

void HDRResolveRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	const ShadingData& shadingData	= viewSpec.GetData().Get<ShadingData>();
	HDRResolvePassData& passData	= viewSpec.GetData().Create<HDRResolvePassData>();
	
	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingResolution();
	const math::Vector3u textureRes(renderingRes.x(), renderingRes.y(), 1);
	
	rhi::TextureDefinition tonemappedTextureDef;
	tonemappedTextureDef.resolution = textureRes;
	tonemappedTextureDef.usage		= lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferSource);
	tonemappedTextureDef.format		= rhi::EFragmentFormat::RGBA8_UN_Float;
	passData.tonemappedTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("TonemappedTexture"), tonemappedTextureDef, rhi::EMemoryUsage::GPUOnly);

	tonemapping::TonemappingSettings tonemappingSettings;
	tonemappingSettings.textureSize		= renderingRes;
	tonemappingSettings.invTextureSize	= math::Vector2f(1.f / static_cast<Real32>(renderingRes.x()), 1.f / static_cast<Real32>(renderingRes.y()));

	tonemapping::DoTonemappingAndGammaCorrection(graphBuilder, viewSpec, shadingData.radiance, passData.tonemappedTexture, tonemappingSettings);
}

} // spt::rsc
