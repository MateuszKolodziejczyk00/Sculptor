#include "HDRResolveRenderStage.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ResourcesManager.h"
#include "Common/ShaderCompilationInput.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "View/ViewRenderingSpec.h"
#include "View/RenderView.h"
#include "Engine.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "Transfers/UploadUtils.h"
#include "EngineFrame.h"
#include "Camera/CameraSettings.h"
#include "RenderScene.h"
#include "SceneRenderer/Utils/AutomaticExposure.h"
#include "Loaders/TextureLoader.h"
#include "Paths.h"

namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::HDRResolve, HDRResolveRenderStage);

//////////////////////////////////////////////////////////////////////////////////////////////////
// Paramters =====================================================================================

namespace params
{

RendererFloatParameter minLogLuminance("Min Log Luminance", { "Exposure" }, -10.f, -20.f, 20.f);
RendererFloatParameter maxLogLuminance("Max Log Luminance", { "Exposure" }, 20.f, -20.f, 20.f);

RendererBoolParameter enableBloom("Enable Bloom", { "Bloom" }, true);
RendererFloatParameter bloomIntensity("Bloom Intensity", { "Bloom" }, 1.0f, 0.f, 10.f);
RendererFloatParameter bloomBlendFactor("Bloom Blend Factor", { "Bloom" }, 0.35f, 0.f, 1.f);
RendererFloatParameter bloomUpsampleBlendFactor("Bloom Upsample Blend Factor", { "Bloom" }, 0.45f, 0.f, 1.f);

RendererBoolParameter enableLensFlares("Enable Lens Flares", { "Lens Flares" }, true);
RendererFloatParameter lensFlaresIntensity("Lens Flares Intensity", { "Lens Flares" }, 0.0006f, 0.f, 1.f);

RendererFloat3Parameter lensFlaresColor("Lens Flares Color", { "Lens Flares" }, math::Vector3f(1.f, 1.f, 1.f), 0.f, 1.f);

RendererIntParameter lensFlaresGhostsNum("Lens Flares GhostsNum", { "Lens Flares", "Ghosts" }, 6, 0, 10);
RendererFloatParameter lensFlaresGhostsDispersal("Lens Flares Ghosts Dispersal", { "Lens Flares", "Ghosts" }, 0.3f, 0.f, 1.f);
RendererFloatParameter lensFlaresGhostsIntensity("Lens Flares Ghosts Intensity", { "Lens Flares", "Ghosts" }, 1.f, 0.f, 1.f);
RendererFloatParameter lensFlaresGhostsDistortion("Lens Flares Ghosts Distortion", { "Lens Flares", "Ghosts" }, 0.01f, 0.f, 1.f);

RendererFloatParameter lensFlaresHaloIntensity("Lens Flares Halo Intensity", { "Lens Flares", "Halo" }, 1.f, 0.f, 1.f);
RendererFloatParameter lensFlaresHaloDistortion("Lens Flares Halo Distortion", { "Lens Flares", "Halo" }, 0.01f, 0.f, 1.f);
RendererFloatParameter lensFlaresHaloWidth("Lens Flares Halo Width", { "Lens Flares", "Halo" }, 0.55f, 0.f, 1.f);

RendererBoolParameter enableColorDithering("Enable Color Dithering", { "Post Process" }, true);

RendererFloatParameter detailStrength("Detail Strength", { "LocalTonemap" }, 1.f, 0.f, 2.f);
RendererFloatParameter contrastStrength("Contrast Strength", { "LocalTonemap" }, 0.8f, 0.f, 2.f);

RendererFloatParameter bilateralGridStrength("Bilateral Grid Strength", { "LocalTonemap" }, 0.5f, 0.f, 1.f);

} // params

//////////////////////////////////////////////////////////////////////////////////////////////////
// Lens Flares ===================================================================================

namespace lens_flares
{

BEGIN_SHADER_STRUCT(LensFlaresParams)
	SHADER_STRUCT_FIELD(math::Vector3f, ghostsDistortion)
	SHADER_STRUCT_FIELD(Uint32,			ghostsNum)
	SHADER_STRUCT_FIELD(math::Vector3f,	haloDistortion)
	SHADER_STRUCT_FIELD(Real32,			ghostsInensity)
	SHADER_STRUCT_FIELD(math::Vector3f,	lensFlaresColor)
	SHADER_STRUCT_FIELD(Real32,			ghostsDispersal)
	SHADER_STRUCT_FIELD(Real32,			ghostsIntensity)
	SHADER_STRUCT_FIELD(Real32,			haloIntensity)
	SHADER_STRUCT_FIELD(Real32,			haloWidth)
END_SHADER_STRUCT();


DS_BEGIN(LensFlaresPassDS, rg::RGDescriptorSetState<LensFlaresPassDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_inputTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),                            u_outputTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<LensFlaresParams>),                       u_lensFlaresParams)
DS_END();


BEGIN_SHADER_STRUCT(LensFlaresBlurParams)
	SHADER_STRUCT_FIELD(Uint32, isHorizontal)
END_SHADER_STRUCT();


DS_BEGIN(LensFlaresBlurDS, rg::RGDescriptorSetState<LensFlaresBlurDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_inputTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_inputSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),                            u_outputTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<LensFlaresBlurParams>),                   u_blurParams)
DS_END();


static rdr::PipelineStateID CompileComputeLensFlaresPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/LensFlares.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ComputeLensFlaresCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("ComputeLensFlaresPipeline"), shader);
}


static rdr::PipelineStateID CompileLensFlaresBlurPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/LensFlaresBlur.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "LensFlaresBlurCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("LensFlaresBlurPipeline"), shader);
}


static rg::RGTextureViewHandle ComputeLensFlares(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, rg::RGTextureViewHandle bloomTexture)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector3u lensFlaresRes = bloomTexture->GetResolution();

	const rg::RGTextureViewHandle lensFlaresTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Lens Flares Texture"), rg::TextureDef(lensFlaresRes, SceneRendererStatics::hdrFormat));
	
	const rg::RGTextureViewHandle tempTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Lens Flares Texture Temp"), rg::TextureDef(lensFlaresRes, SceneRendererStatics::hdrFormat));

	static const rdr::PipelineStateID computeLensFlaresPipeline = CompileComputeLensFlaresPipeline();

	LensFlaresParams lensFlaresParams;
	lensFlaresParams.ghostsDistortion		= math::Vector3f(-params::lensFlaresGhostsDistortion, 0.f, params::lensFlaresGhostsDistortion);
	lensFlaresParams.ghostsNum				= params::lensFlaresGhostsNum;
	lensFlaresParams.haloDistortion			= math::Vector3f(-params::lensFlaresHaloDistortion, 0.f, params::lensFlaresHaloDistortion);
	lensFlaresParams.ghostsInensity			= params::lensFlaresGhostsIntensity;
	lensFlaresParams.lensFlaresColor		= params::lensFlaresColor;
	lensFlaresParams.ghostsDispersal		= params::lensFlaresGhostsDispersal;
	lensFlaresParams.ghostsIntensity		= params::lensFlaresGhostsIntensity;
	lensFlaresParams.haloIntensity			= params::lensFlaresHaloIntensity;
	lensFlaresParams.haloWidth				= params::lensFlaresHaloWidth;

	const lib::MTHandle<LensFlaresPassDS> lensFlaresPassDS = graphBuilder.CreateDescriptorSet<LensFlaresPassDS>(RENDERER_RESOURCE_NAME("LensFlaresPassDS"));
	lensFlaresPassDS->u_inputTexture     = bloomTexture;
	lensFlaresPassDS->u_outputTexture    = lensFlaresTexture;
	lensFlaresPassDS->u_lensFlaresParams = lensFlaresParams;
	
	graphBuilder.Dispatch(RG_DEBUG_NAME("Apply Lens Flares"),
						  computeLensFlaresPipeline,
						  math::Utils::DivideCeil(lensFlaresRes, math::Vector3u(8u, 8u, 1u)),
						  rg::BindDescriptorSets(lensFlaresPassDS));

	static const rdr::PipelineStateID lensFlaresBlurPipeline = CompileLensFlaresBlurPipeline();

	const lib::MTHandle<LensFlaresBlurDS> blurHorizontalPassDS = graphBuilder.CreateDescriptorSet<LensFlaresBlurDS>(RENDERER_RESOURCE_NAME("LensFlaresHorizontalBlurDS"));
	blurHorizontalPassDS->u_inputTexture	= lensFlaresTexture;
	blurHorizontalPassDS->u_outputTexture	= tempTexture;
	blurHorizontalPassDS->u_blurParams		= LensFlaresBlurParams{ 1u };

	graphBuilder.Dispatch(RG_DEBUG_NAME("Lens Flares Horizontal Blur"),
						  lensFlaresBlurPipeline,
						  math::Utils::DivideCeil(lensFlaresRes, math::Vector3u(256u, 1u, 1u)),
						  rg::BindDescriptorSets(blurHorizontalPassDS));

	const lib::MTHandle<LensFlaresBlurDS> blurVerticalPassDS = graphBuilder.CreateDescriptorSet<LensFlaresBlurDS>(RENDERER_RESOURCE_NAME("LensFlaresVericalBlurDS"));
	blurVerticalPassDS->u_inputTexture		= tempTexture;
	blurVerticalPassDS->u_outputTexture		= lensFlaresTexture;
	blurVerticalPassDS->u_blurParams		= LensFlaresBlurParams{ 0u };

	graphBuilder.Dispatch(RG_DEBUG_NAME("Lens Flares Vertical Blur"),
						  lensFlaresBlurPipeline,
						  math::Utils::DivideCeil(math::Vector3u(lensFlaresRes.y(), lensFlaresRes.x(), lensFlaresRes.z()), math::Vector3u(256u, 1u, 1u)),
						  rg::BindDescriptorSets(blurVerticalPassDS));

	return lensFlaresTexture;
}

} // lens_flares

//////////////////////////////////////////////////////////////////////////////////////////////////
// Bloom =========================================================================================

namespace bloom
{

BEGIN_SHADER_STRUCT(BloomPassInfo)
	SHADER_STRUCT_FIELD(math::Vector2f, inputPixelSize)
	SHADER_STRUCT_FIELD(math::Vector2f, outputPixelSize)
	SHADER_STRUCT_FIELD(Real32,			bloomUpsampleBlendFactor)
	SHADER_STRUCT_FIELD(Real32,			bloomIntensity)
	SHADER_STRUCT_FIELD(Bool,			isSetupPass)
END_SHADER_STRUCT();


DS_BEGIN(BloomPassDS, rg::RGDescriptorSetState<BloomPassDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<BloomPassInfo>),                          u_bloomInfo)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                           u_inputTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                            u_outputTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_linearSampler)
DS_END();


BEGIN_SHADER_STRUCT(BloomCompositePassInfo)
	SHADER_STRUCT_FIELD(math::Vector3f,	lensDirtThreshold)
	SHADER_STRUCT_FIELD(Bool,			hasLensDirtTexture)
	SHADER_STRUCT_FIELD(math::Vector3f,	lensDirtIntensity)
	SHADER_STRUCT_FIELD(Real32,			bloomBlendFactor)
	SHADER_STRUCT_FIELD(Real32,			lensFlaresIntensity)
	SHADER_STRUCT_FIELD(Bool,			hasLensFlaresTexture)
END_SHADER_STRUCT()


DS_BEGIN(BloomCompositePassDS, rg::RGDescriptorSetState<BloomCompositePassDS>)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<math::Vector4f>),   u_lensDirtTexture)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<math::Vector4f>),   u_lensFlaresTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<BloomCompositePassInfo>), u_bloomCompositeInfo)
DS_END();


BloomPassInfo CreateBloomPassInfo(math::Vector2u inputRes, math::Vector2u outputRes)
{
	BloomPassInfo passInfo;
	passInfo.inputPixelSize				= inputRes.cast<Real32>().cwiseInverse();
	passInfo.outputPixelSize			= outputRes.cast<Real32>().cwiseInverse();
	passInfo.bloomUpsampleBlendFactor	= params::bloomUpsampleBlendFactor;
	passInfo.bloomIntensity				= params::bloomIntensity;

	return passInfo;
}


static rdr::PipelineStateID CompileBloomDownsamplePipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/Bloom.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "BloomDownsampleCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("BloomDownsamplePipeline"), shader);
}


static rdr::PipelineStateID CompileBloomUpsamplePipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/Bloom.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "BloomUpsampleCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("BloomUpsamplePipeline"), shader);
}


static rdr::PipelineStateID CompileBloomCompositePipeline()
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("BLOOM_COMPOSITE", "1"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/Bloom.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "BloomCompositeCS"), compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("BloomUpsamplePipeline"), shader);
}


static void BloomDownsample(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, const lib::DynamicArray<rg::RGTextureViewHandle>& bloomTextureMips, rg::RGTextureViewHandle inputTexture)
{
	SPT_PROFILER_FUNCTION();
	
	const math::Vector2u resolution = viewSpec.GetRenderView().GetOutputRes();

	const Int32 bloomPassesNum = static_cast<Int32>(bloomTextureMips.size());

	static const rdr::PipelineStateID downsamplePipeline = CompileBloomDownsamplePipeline();

	rg::RGTextureViewHandle inputTextureView = inputTexture;

	for (Int32 passIdx = 0; passIdx < bloomPassesNum; ++passIdx)
	{
		rg::RGTextureViewHandle outputTextureView = bloomTextureMips[passIdx];

		const math::Vector2u inputRes  = inputTextureView->GetResolution2D();
		const math::Vector2u outputRes = outputTextureView->GetResolution2D();

		BloomPassInfo passInfo = CreateBloomPassInfo(inputRes, outputRes);
		if (passIdx == 0)
		{
			passInfo.isSetupPass = true;
		}

		const lib::MTHandle<BloomPassDS> bloomPassDS = graphBuilder.CreateDescriptorSet<BloomPassDS>(RENDERER_RESOURCE_NAME(std::format("BloomDownsampleDS(%d)", passIdx)));
		bloomPassDS->u_bloomInfo		= passInfo;
		bloomPassDS->u_inputTexture		= inputTextureView;
		bloomPassDS->u_outputTexture	= outputTextureView;

		const math::Vector3u groupCount(math::Utils::DivideCeil(outputRes.x(), 8u), math::Utils::DivideCeil(outputRes.y(), 8u), 1u);
		graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("Bloom Downsample [{}, {}] -> [{}, {}]", inputRes.x(), inputRes.y(), outputRes.x(), outputRes.y())),
							  downsamplePipeline, 
							  groupCount,
							  rg::BindDescriptorSets(bloomPassDS));

		inputTextureView = outputTextureView;
	}
}

static void BloomUpsample(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, const lib::DynamicArray<rg::RGTextureViewHandle>& bloomTextureMips)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = viewSpec.GetRenderView().GetOutputRes();

	const Int32 bloomPassesNum = static_cast<Int32>(bloomTextureMips.size());

	static const rdr::PipelineStateID upsamplePipeline = CompileBloomUpsamplePipeline();

	rg::RGTextureViewHandle inputTextureView = bloomTextureMips.back();

	// Upsample passes
	for (Int32 passIdx = static_cast<Int32>(bloomPassesNum) - 2; passIdx >= 0; --passIdx)
	{
		rg::RGTextureViewHandle outputTextureView = bloomTextureMips[passIdx];

		const math::Vector2u inputRes  = inputTextureView->GetResolution2D();
		const math::Vector2u outputRes = outputTextureView->GetResolution2D();

		BloomPassInfo passInfo = CreateBloomPassInfo(inputRes, outputRes);

		const lib::MTHandle<BloomPassDS> bloomPassDS = graphBuilder.CreateDescriptorSet<BloomPassDS>(RENDERER_RESOURCE_NAME(std::format("BloomUpsampleDS(%d)", passIdx)));
		bloomPassDS->u_bloomInfo		= passInfo;
		bloomPassDS->u_inputTexture		= inputTextureView;
		bloomPassDS->u_outputTexture	= outputTextureView;

		const math::Vector3u groupCount(math::Utils::DivideCeil(outputRes.x(), 8u), math::Utils::DivideCeil(outputRes.y(), 8u), 1u);
		graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("Bloom Upsample [{}, {}] -> [{}, {}]", inputRes.x(), inputRes.y(), outputRes.x(), outputRes.y())),
							  upsamplePipeline, 
							  groupCount,
							  rg::BindDescriptorSets(bloomPassDS));

		inputTextureView = outputTextureView;
	}
}

static void BloomComposite(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, rg::RGTextureViewHandle bloomTextureMip0, rg::RGTextureViewHandle lensFlaresTexture, rg::RGTextureViewHandle outputTexture)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const math::Vector2u resolution = renderView.GetOutputRes();

	static const rdr::PipelineStateID combinePipeline = CompileBloomCompositePipeline();

	const math::Vector2u inputRes = math::Vector2u(resolution.x() >> 1, resolution.y() >> 1);
	const math::Vector2u outputRes = math::Vector2u(resolution.x(), resolution.y());

	const BloomPassInfo passInfo = CreateBloomPassInfo(inputRes, outputRes);

	const lib::MTHandle<BloomPassDS> bloomPassDS = graphBuilder.CreateDescriptorSet<BloomPassDS>(RENDERER_RESOURCE_NAME("BloomCompositeDS"));
	bloomPassDS->u_inputTexture		= bloomTextureMip0;
	bloomPassDS->u_outputTexture	= outputTexture;
	bloomPassDS->u_bloomInfo		= passInfo;

	BloomCompositePassInfo compositePassInfo;
	compositePassInfo.bloomBlendFactor		= params::bloomBlendFactor;
	compositePassInfo.lensFlaresIntensity	= params::lensFlaresIntensity;

	const lib::MTHandle<BloomCompositePassDS> bloomCompositePassDS = graphBuilder.CreateDescriptorSet<BloomCompositePassDS>(RENDERER_RESOURCE_NAME("BloomCombinePassDS"));

	if (const CameraLensSettingsComponent* lensSettings = renderView.GetBlackboard().Find<CameraLensSettingsComponent>())
	{
		bloomCompositePassDS->u_lensDirtTexture = lensSettings->lensDirtTexture;
		compositePassInfo.hasLensDirtTexture	= true;
		compositePassInfo.lensDirtThreshold		= lensSettings->lensDirtThreshold;
		compositePassInfo.lensDirtIntensity		= lensSettings->lensDirtIntensity;
	}

	bloomCompositePassDS->u_lensFlaresTexture = lensFlaresTexture;
	compositePassInfo.hasLensFlaresTexture    = lensFlaresTexture.IsValid();

	bloomCompositePassDS->u_bloomCompositeInfo = compositePassInfo;

	const math::Vector3u groupCount(math::Utils::DivideCeil(resolution.x(), 8u), math::Utils::DivideCeil(resolution.y(), 8u), 1u);
	graphBuilder.Dispatch(RG_DEBUG_NAME("Bloom Composite"),
						  combinePipeline, 
						  groupCount,
						  rg::BindDescriptorSets(bloomPassDS, bloomCompositePassDS));
}

static void ApplyBloom(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, rg::RGTextureViewHandle linearColorTexture)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "Bloom");

	const math::Vector2u resolution = viewSpec.GetRenderView().GetOutputRes();

	const Uint32 bloomPassesNum = std::max(math::Utils::ComputeMipLevelsNumForResolution(resolution), 6u) - 4u;

	rg::TextureDef bloomTextureDef;
	bloomTextureDef.resolution	= math::Vector3u(resolution.x() / 2, resolution.y() / 2, 1);
	bloomTextureDef.format		= SceneRendererStatics::hdrFormat;
	bloomTextureDef.samples		= 1;
	bloomTextureDef.mipLevels	= bloomPassesNum;
	bloomTextureDef.arrayLayers	= 1;
	const rg::RGTextureHandle bloomTexture = graphBuilder.CreateTexture(RG_DEBUG_NAME("Bloom Downsample Texture"), bloomTextureDef);

	lib::DynamicArray<rg::RGTextureViewHandle> bloomTextureMips;
	bloomTextureMips.reserve(static_cast<SizeType>(bloomPassesNum));
	for (SizeType mipIdx = 0; mipIdx < bloomPassesNum; ++mipIdx)
	{
		rhi::TextureViewDefinition viewInfo;
		viewInfo.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color, static_cast<Uint32>(mipIdx), 1);

		bloomTextureMips.emplace_back(graphBuilder.CreateTextureView(RG_DEBUG_NAME(std::format("Bloom Texture Mip {}", mipIdx)), bloomTexture, viewInfo));
	}

	BloomDownsample(graphBuilder, viewSpec, bloomTextureMips, linearColorTexture);

	BloomUpsample(graphBuilder, viewSpec, bloomTextureMips);

	const rg::RGTextureViewHandle lensFlaresTexture = params::enableLensFlares
													? lens_flares::ComputeLensFlares(graphBuilder, viewSpec, bloomTextureMips[0])
													: rg::RGTextureViewHandle{};

	BloomComposite(graphBuilder, viewSpec, bloomTextureMips[0], lensFlaresTexture, linearColorTexture);
}

} // bloom

//////////////////////////////////////////////////////////////////////////////////////////////////
// Tonemapping ===================================================================================

namespace tonemapping
{

BEGIN_SHADER_STRUCT(TonemappingPassConstants)
	SHADER_STRUCT_FIELD(math::Vector2f, bilateralGridUVPerPixel)
	SHADER_STRUCT_FIELD(Bool,           enableColorDithering)
	SHADER_STRUCT_FIELD(Real32,         minLogLuminance)
	SHADER_STRUCT_FIELD(Real32,         logLuminanceRange)
	SHADER_STRUCT_FIELD(Real32,         inverseLogLuminanceRange)
	SHADER_STRUCT_FIELD(Real32,         contrastStrength)
	SHADER_STRUCT_FIELD(Real32,         detailStrength)
	SHADER_STRUCT_FIELD(Real32,         bilateralGridStrength)
END_SHADER_STRUCT();


DS_BEGIN(TonemappingDS, rg::RGDescriptorSetState<TonemappingDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),								u_linearColorTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),								u_LDRTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),	u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture3DBinding<math::Vector2f>),								u_luminanceBilateralGridTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_logLuminanceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture3DBinding<math::Vector3f>),								u_tonemappingLUT)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<TonemappingPassConstants>),					u_tonemappingConstants)
DS_END();


static rdr::PipelineStateID CompileTonemappingPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/Tonemapping.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "TonemappingCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("TonemappingPipeline"), shader);
}


struct TonemappingParameters
{
	rg::RGTextureViewHandle linearColorTexture;
	rg::RGTextureViewHandle ldrTexture;
	rg::RGTextureViewHandle luminanceBilateralGridTexture;
	rg::RGTextureViewHandle logLuminanceTexture;
	rg::RGTextureViewHandle tonemappingLUT;
	TonemappingPassConstants tonemappingConstants;
};


static void DoTonemappingAndGammaCorrection(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, const TonemappingParameters& tonemappingParameters)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(tonemappingParameters.linearColorTexture.IsValid());
	SPT_CHECK(tonemappingParameters.ldrTexture.IsValid());
	SPT_CHECK(tonemappingParameters.luminanceBilateralGridTexture.IsValid());
	SPT_CHECK(tonemappingParameters.logLuminanceTexture.IsValid());
	SPT_CHECK(tonemappingParameters.tonemappingLUT.IsValid());

	const RenderView& renderView = viewSpec.GetRenderView();
	
	const lib::MTHandle<TonemappingDS> tonemappingDS = graphBuilder.CreateDescriptorSet<TonemappingDS>(RENDERER_RESOURCE_NAME("TonemappingDS"));
	tonemappingDS->u_linearColorTexture            = tonemappingParameters.linearColorTexture;
	tonemappingDS->u_LDRTexture                    = tonemappingParameters.ldrTexture;
	tonemappingDS->u_luminanceBilateralGridTexture = tonemappingParameters.luminanceBilateralGridTexture;
	tonemappingDS->u_logLuminanceTexture           = tonemappingParameters.logLuminanceTexture;
	tonemappingDS->u_tonemappingLUT                = tonemappingParameters.tonemappingLUT;
	tonemappingDS->u_tonemappingConstants          = tonemappingParameters.tonemappingConstants;

	static const rdr::PipelineStateID pipelineState = CompileTonemappingPipeline();

	const math::Vector2u resolution = renderView.GetOutputRes();

	const math::Vector3u dispatchGroupsNum(math::Utils::DivideCeil(resolution.x(), 8u), math::Utils::DivideCeil(resolution.y(), 8u), 1u);

	graphBuilder.Dispatch(RG_DEBUG_NAME("Tonemapping And Gamma"),
						  pipelineState,
						  dispatchGroupsNum,
						  rg::BindDescriptorSets(tonemappingDS,
												 renderView.GetRenderViewDS()));
}

} // tonemapping

//////////////////////////////////////////////////////////////////////////////////////////////////
// gamma =========================================================================================

namespace gamma
{

DS_BEGIN(GammaCorrectionDS, rg::RGDescriptorSetState<GammaCorrectionDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>), u_texture)
DS_END();


rdr::PipelineStateID CompileGammaCorrectionPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/GammaCorrection.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "GammaCorrectionCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("GammaCorrectionPipeline"), shader);
}

void DoGammaCorrection(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle texture)
{
	SPT_PROFILER_FUNCTION();
	
	const lib::MTHandle<GammaCorrectionDS> gammaCorrectionCS = graphBuilder.CreateDescriptorSet<GammaCorrectionDS>(RENDERER_RESOURCE_NAME("GammaCorrectionDS"));
	gammaCorrectionCS->u_texture = texture;

	static const rdr::PipelineStateID pipelineState = CompileGammaCorrectionPipeline();

	const math::Vector2u textureRes = texture->GetResolution2D();
	const math::Vector3u dispatchGroupsNum(math::Utils::DivideCeil(textureRes.x(), 8u), math::Utils::DivideCeil(textureRes.y(), 8u), 1);
	graphBuilder.Dispatch(RG_DEBUG_NAME("Gamma Correction"), pipelineState, dispatchGroupsNum, rg::BindDescriptorSets(gammaCorrectionCS));
}

} // gamma

//////////////////////////////////////////////////////////////////////////////////////////////////
// HDRResolveRenderStage =========================================================================

HDRResolveRenderStage::HDRResolveRenderStage()
{ }

void HDRResolveRenderStage::Initialize(RenderView& renderView)
{
	SPT_PROFILER_FUNCTION();

	Super::Initialize(renderView);

	rhi::BufferDefinition bufferDef;
	bufferDef.size  = sizeof(ViewExposureData);
	bufferDef.usage = lib::Flags(rhi::EBufferUsage::Uniform, rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferSrc, rhi::EBufferUsage::TransferDst);
	m_viewExposureBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("View Exposure Buffer"), bufferDef, rhi::EMemoryUsage::GPUOnly);

	const Real32 defaultExposure = 1.f;
	gfx::FillBuffer(lib::Ref(m_viewExposureBuffer), 0u, bufferDef.size, *reinterpret_cast<const Uint32*>(&defaultExposure));

	renderView.SetExposureDataBuffer(m_viewExposureBuffer);

	lib::SharedPtr<rdr::Texture> lut = gfx::TextureLoader::LoadTexture(engn::Paths::GetContentPath() + "/RenderingPipeline/Textures/tony_mc_mapface.dds");
	SPT_CHECK(!!lut);

	m_tonemappingLUT = lut->CreateView(RENDERER_RESOURCE_NAME("TonyMcMapface LUT View"));
}

void HDRResolveRenderStage::Deinitialize(RenderView& renderView)
{
	SPT_PROFILER_FUNCTION();

	Super::Deinitialize(renderView);

	renderView.ResetExposureDataBuffer();
}

void HDRResolveRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "HDR Resolve");

	ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	const math::Vector2u resolution = viewSpec.GetRenderView().GetOutputRes();
	const math::Vector3u textureRes(resolution.x(), resolution.y(), 1);
	const math::Vector2f inputPixelSize = math::Vector2f(1.f / static_cast<Real32>(resolution.x()), 1.f / static_cast<Real32>(resolution.y()));

	const rg::RGTextureViewHandle tonemappedTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("TonemappedTexture"), rg::TextureDef(textureRes, stageContext.rendererSettings.outputFormat));

	const rg::RGTextureViewHandle linearColorTexture = viewContext.luminance;

	const Real32 minLogLuminance = params::minLogLuminance;
	const Real32 logLuminanceRange = params::maxLogLuminance - params::minLogLuminance;

	const rg::RGBufferViewHandle viewExposureData = graphBuilder.AcquireExternalBufferView(m_viewExposureBuffer->CreateFullView());

	automatic_exposure::Inputs automaticExposureInputs;
	automaticExposureInputs.linearColor      = linearColorTexture;
	automaticExposureInputs.minLogLuminance  = params::minLogLuminance;
	automaticExposureInputs.maxLogLuminance  = params::maxLogLuminance;
	automaticExposureInputs.viewExposureData = viewExposureData;

	const automatic_exposure::Outputs automaticExposureOutputs = automatic_exposure::RenderAutoExposure(graphBuilder, renderScene, viewSpec, automaticExposureInputs);

	if (params::enableBloom)
	{
		bloom::ApplyBloom(graphBuilder, viewSpec, linearColorTexture);
	}

	const math::Vector2u gridTileSize     = automaticExposureOutputs.bilateralGridInfo.tilesSize;
	const math::Vector2u gridResolution2D = automaticExposureOutputs.bilateralGridInfo.bilateralGrid->GetResolution2D();

	const math::Vector2u gridSize2D = gridTileSize.cwiseProduct(gridResolution2D);

	tonemapping::TonemappingPassConstants tonemappingSettings;
	tonemappingSettings.enableColorDithering     = params::enableColorDithering;
	tonemappingSettings.minLogLuminance          = minLogLuminance;
	tonemappingSettings.logLuminanceRange        = logLuminanceRange;
	tonemappingSettings.inverseLogLuminanceRange = 1.f / logLuminanceRange;
	tonemappingSettings.contrastStrength         = params::contrastStrength;
	tonemappingSettings.detailStrength           = params::detailStrength;
	tonemappingSettings.bilateralGridStrength    = params::bilateralGridStrength;
	tonemappingSettings.bilateralGridUVPerPixel  = gridSize2D.cast<Real32>().cwiseInverse();

	tonemapping::TonemappingParameters tonemappingParameters;
	tonemappingParameters.linearColorTexture            = linearColorTexture;
	tonemappingParameters.ldrTexture                    = tonemappedTexture;
	tonemappingParameters.luminanceBilateralGridTexture = automaticExposureOutputs.bilateralGridInfo.bilateralGrid;
	tonemappingParameters.logLuminanceTexture           = automaticExposureOutputs.bilateralGridInfo.downsampledLogLuminance;
	tonemappingParameters.tonemappingLUT                = graphBuilder.AcquireExternalTextureView(m_tonemappingLUT);
	tonemappingParameters.tonemappingConstants          = tonemappingSettings;

	tonemapping::DoTonemappingAndGammaCorrection(graphBuilder, viewSpec, tonemappingParameters);

	viewContext.output = tonemappedTexture;
	
	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
	
	RenderViewEntryContext context;
	context.Bind<RenderViewEntryDelegates::RenderSceneDebugLayerData>(RenderViewEntryDelegates::RenderSceneDebugLayerData{ tonemappedTexture });
	viewSpec.GetRenderViewEntry(RenderViewEntryDelegates::RenderSceneDebugLayer).Broadcast(graphBuilder, renderScene, viewSpec, context);
}

} // spt::rsc
