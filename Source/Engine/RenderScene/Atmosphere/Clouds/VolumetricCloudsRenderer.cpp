#include "VolumetricCloudsRenderer.h"
#include "Loaders/TextureLoader.h"
#include "CloudsNoiseTexturesGenerator.h"
#include "Atmosphere/AtmosphereSceneSubsystem.h"
#include "RenderScene.h"
#include "RenderGraphBuilder.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/ConstantBufferRefBinding.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "Paths.h"
#include "GlobalResources/GlobalResources.h"
#include "GlobalResources/GlobalResourcesRegistry.h"
#include "EngineFrame.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "Lights/ViewShadingInput.h"


SPT_DEFINE_LOG_CATEGORY(VolumetricCloudsRenderer, true);

namespace spt::rsc::clouds
{

namespace renderer_params
{

RendererFloatParameter baseShapeNoiseMeters("Base Shape Noise Meters", { "Clouds" }, 800.f, 0.f, 60000.f);
RendererFloatParameter cirrusCloudsMeters("Cirrus Clouds Meters", { "Clouds" }, 40000.f, 0.f, 60000.f);

RendererFloatParameter detailShapeNoiseStrength0("Detail Shape Noise Strength (Octave 0)", { "Clouds" }, 0.45f, 0.f, 2.f);
RendererFloatParameter detailShapeNoiseMeters0("Detail Shape Noise Meters (Octave 0)", { "Clouds" }, 255.f, 0.f, 20000.f);
RendererFloatParameter detailShapeNoiseStrength1("Detail Shape Noise Strength (Octave 1)", { "Clouds" }, 0.0f, 0.f, 2.f);
RendererFloatParameter detailShapeNoiseMeters1("Detail Shape Noise Meters (Octave 1)", { "Clouds" }, 255.f, 0.f, 20000.f);

RendererFloatParameter weatherMapMeters("Weather Map meters", { "Clouds" }, 16000.f, 0.f, 50000.f);

RendererFloatParameter curlNoiseOffset("Curl Noise Offset", { "Clouds" }, 5.f, 0.f, 500.f);
RendererFloatParameter curlNoiseMeters("Curl Noise Meters", { "Clouds" }, 2222.f, 0.f, 40000.f);

RendererFloatParameter globalDensity("Global Density", { "Clouds" }, 1.f, 0.f, 5.f);
RendererFloatParameter globalCoverageOffset("Global Coverage Offset", { "Clouds" }, 0.4f, -1.f, 1.f);
RendererFloatParameter cloudsHeightOffset("Clouds Height Offset", { "Clouds" }, 0.0f, -1.f, 1.f);

RendererFloatParameter globalCoverageMultiplier("Global Coverage Multiplier", { "Clouds" }, 1.f, -2.f, 2.f);
RendererFloatParameter cloudsHeightMultiplier("Clouds Height Multiplier", { "Clouds" }, 1.0f, -2.f, 2.f);

RendererBoolParameter enableVolumetricClouds("Enable Volumetric Clouds", { "Clouds" }, true);

RendererBoolParameter forceTransmittanceMapFullUpdates("Force Transmittance Map Full Updates", { "Clouds" }, false);
RendererBoolParameter forceHRCloudsProbeFullUpdates("Force High Res Clouds Probe Full Updates", { "Clouds" }, false);
RendererBoolParameter forceLRCloudsProbeFullUpdates("Force Low Res Clouds Probe Full Updates", { "Clouds" }, false);
RendererBoolParameter fullResClouds("Full Res Clouds", { "Clouds" }, false);

} // renderer_params


static void BuildCloudsTransmittanceMapMatrices(const math::Vector3f& center, const math::Vector3f& lightDirection, Real32 cloudscapeRange, Real32 cloudscapeMaxHeight, math::Matrix4f& outViewProj)
{
	const math::Quaternion rotation = math::Quaternionf::FromTwoVectors(math::Vector3f(1.f, 0.f, 0.f), lightDirection);

	const math::Matrix3f rotationMatrix = rotation.toRotationMatrix();

	const math::Matrix3f invRotationMatrix = rotationMatrix.transpose();

	math::Matrix4f view = math::Matrix4f::Zero();
	view.topLeftCorner<3, 3>() = invRotationMatrix;
	view.topRightCorner<3, 1>() = invRotationMatrix * -center;
	view(3, 3) = 1.f;

	math::ParametrizedLine<Real32, 3> line(center, lightDirection);

	const math::Vector3f edges[5] =
	{
		center + math::Vector3f( cloudscapeRange,  cloudscapeRange, 0.f),
		center + math::Vector3f( cloudscapeRange, -cloudscapeRange, 0.f),
		center + math::Vector3f(-cloudscapeRange,  cloudscapeRange, 0.f),
		center + math::Vector3f(-cloudscapeRange, -cloudscapeRange, 0.f),
		center + math::Vector3f(0.f, 0.f, cloudscapeMaxHeight)
	};

	math::Vector2f rightTop = math::Vector2f::Zero();
	math::Vector2f leftBottom = math::Vector2f{
		std::numeric_limits<Real32>::max(),
		std::numeric_limits<Real32>::max()
	};

	for (const math::Vector3f& edge : edges)
	{
		math::Vector4f viewSpace =  view * math::Vector4f(edge.x(), edge.y(), edge.z(), 1.f);
		viewSpace /= viewSpace.w(); // perspective divide

		rightTop.x() = std::max(rightTop.x(), viewSpace.y());
		rightTop.y() = std::max(rightTop.y(), viewSpace.z());
		leftBottom.x() = std::min(leftBottom.x(), viewSpace.y());
		leftBottom.y() = std::min(leftBottom.y(), viewSpace.z());
	}


	// planes don't matter, it won't be used for culling, 
	const Real32 nearPlane = 0.0001f;
	const Real32 farPlane  = 1.f;

	const math::Matrix4f proj = projection::CreateOrthographicsProjection(nearPlane, farPlane, leftBottom.y(), rightTop.y(), leftBottom.x(), rightTop.x());

	outViewProj = proj * view;
}


namespace render_main_view
{

namespace accumulate
{

struct AccumulateCloudsParams
{
	rg::RGTextureViewHandle tracedClouds;
	rg::RGTextureViewHandle accumulatedCloudsHistory;
	rg::RGTextureViewHandle outAccumulatedClouds;

	rg::RGTextureViewHandle tracedCloudsDepth;
	rg::RGTextureViewHandle accumulatedCloudsDepthHistory;
	rg::RGTextureViewHandle outAccumulatedCloudsDepth;

	rg::RGTextureViewHandle accumulatedAge;
	rg::RGTextureViewHandle outAge;

	lib::MTHandle<RenderViewDS> renderViewDS;

	math::Vector2u tracedPixel2x2 = { 0u, 0u };
};


BEGIN_SHADER_STRUCT(VolumetricCloudsAccumulateConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, rcpResolution)
	SHADER_STRUCT_FIELD(math::Vector2u, tracedPixel2x2)
END_SHADER_STRUCT();


DS_BEGIN(AccumulateVolumetricCloudsDS, rg::RGDescriptorSetState<AccumulateVolumetricCloudsDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<VolumetricCloudsAccumulateConstants>),       u_passConstants)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),    u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearMinClampToEdge>), u_depthSampler)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                              u_tracedClouds)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                              u_accumulatedCloudsHistory)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                               u_rwAccumulatedClouds)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                      u_tracedCloudsDepth)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                      u_accumulatedCloudsDepthHistory)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                                       u_rwAccumulatedCloudsDepth)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                                      u_accumulatedCloudsAge)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Uint32>),                                       u_rwCloudsAge)
DS_END();


static rdr::PipelineStateID CompileAccumulateVolumetricCloudsPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Atmosphere/VolumetricClouds/AccumulateVolumetricClouds.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "AccumulateVolumetricCloudsCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("AccumulateVolumetricCloudsPipeline"), shader);
}


static void AccumulateClouds(rg::RenderGraphBuilder& graphBuilder, const AccumulateCloudsParams& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u cloudsResolution = params.outAccumulatedClouds->GetResolution2D();

	static const rdr::PipelineStateID pipeline = CompileAccumulateVolumetricCloudsPipeline();

	VolumetricCloudsAccumulateConstants shaderConstants;
	shaderConstants.resolution     = cloudsResolution;
	shaderConstants.rcpResolution  = cloudsResolution.cast<Real32>().cwiseInverse();
	shaderConstants.tracedPixel2x2 = params.tracedPixel2x2;

	lib::MTHandle<AccumulateVolumetricCloudsDS> ds = graphBuilder.CreateDescriptorSet<AccumulateVolumetricCloudsDS>(RENDERER_RESOURCE_NAME("AccumulateVolumetricCloudsDS"));
	ds->u_passConstants                 = shaderConstants;
	ds->u_tracedClouds                  = params.tracedClouds;
	ds->u_accumulatedCloudsHistory      = params.accumulatedCloudsHistory;
	ds->u_rwAccumulatedClouds           = params.outAccumulatedClouds;
	ds->u_tracedCloudsDepth             = params.tracedCloudsDepth;
	ds->u_accumulatedCloudsDepthHistory = params.accumulatedCloudsDepthHistory;
	ds->u_rwAccumulatedCloudsDepth      = params.outAccumulatedCloudsDepth;
	ds->u_accumulatedCloudsAge          = params.accumulatedAge;
	ds->u_rwCloudsAge                   = params.outAge;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Accumulate Volumetric Clouds"),
						  pipeline,
						  math::Utils::DivideCeil(cloudsResolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds), params.renderViewDS));
}

} // accumulate

namespace upsample
{

struct UpsampleCloudsParams
{
	rg::RGTextureViewHandle cloudsHalfRes;
	rg::RGTextureViewHandle cloudsDepthHalfRes;

	rg::RGTextureViewHandle clouds;
	rg::RGTextureViewHandle cloudsDepth;

	Uint32 frameIdx = 0u;
};

BEGIN_SHADER_STRUCT(VolumetricCloudsUpsampleConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, rcpResolution)
	SHADER_STRUCT_FIELD(Uint32,         frameIdx)
END_SHADER_STRUCT();


DS_BEGIN(UpsampleVolumetricCloudsDS, rg::RGDescriptorSetState<UpsampleVolumetricCloudsDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<VolumetricCloudsUpsampleConstants>),       u_passConstants)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_blueNoise256)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                            u_cloudsHalfRes)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_cloudsDepthHalfRes)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                             u_rwClouds)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                                     u_rwCloudsDepth)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>), u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),  u_linearSampler)
DS_END();


static rdr::PipelineStateID CompileUpsampleVolumetricCloudsPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Atmosphere/VolumetricClouds/UpsampleVolumetricClouds.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "UpsampleVolumetricCloudsCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("UpsampleVolumetricCloudsPipeline"), shader);
}


static void UpsampleClouds(rg::RenderGraphBuilder& graphBuilder, const UpsampleCloudsParams& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u cloudsResolution = params.clouds->GetResolution2D();

	static const rdr::PipelineStateID pipeline = CompileUpsampleVolumetricCloudsPipeline();

	VolumetricCloudsUpsampleConstants shaderConstants;
	shaderConstants.resolution    = cloudsResolution;
	shaderConstants.rcpResolution = cloudsResolution.cast<Real32>().cwiseInverse();
	shaderConstants.frameIdx      = params.frameIdx;

	lib::MTHandle<UpsampleVolumetricCloudsDS> ds = graphBuilder.CreateDescriptorSet<UpsampleVolumetricCloudsDS>(RENDERER_RESOURCE_NAME("UpsampleVolumetricCloudsDS"));
	ds->u_passConstants      = shaderConstants;
	ds->u_blueNoise256       = gfx::global::Resources::Get().blueNoise256.GetView();
	ds->u_cloudsHalfRes      = params.cloudsHalfRes;
	ds->u_cloudsDepthHalfRes = params.cloudsDepthHalfRes;
	ds->u_rwClouds           = params.clouds;
	ds->u_rwCloudsDepth      = params.cloudsDepth;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Upsample Volumetric Clouds"),
						  pipeline,
						  math::Utils::DivideCeil(cloudsResolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds)));
}

} // upsample

struct VolumetricCloudsParams
{
	const CloudscapeContext* cloudscape = nullptr;

	rg::RGTextureViewHandle accumulatedClouds;
	rg::RGTextureViewHandle accumulatedCloudsDepth;
	rg::RGTextureViewHandle accumulatedCloudsAge;
	rg::RGTextureViewHandle accumulatedCloudsHistory;
	rg::RGTextureViewHandle accumulatedCloudsDepthHistory;
	rg::RGTextureViewHandle accumulatedCloudsAgeHistory;
};


BEGIN_SHADER_STRUCT(VolumetricCloudsMainViewConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, rcpResolution)
	SHADER_STRUCT_FIELD(Uint32,         frameIdx)
	SHADER_STRUCT_FIELD(Bool,           fullResTrace)
	SHADER_STRUCT_FIELD(math::Vector2u, tracedPixel2x2)
END_SHADER_STRUCT();


DS_BEGIN(RenderVolumetricCloudsMainViewDS, rg::RGDescriptorSetState<RenderVolumetricCloudsMainViewDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<VolumetricCloudsMainViewConstants>),         u_passConstants)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                      u_blueNoise256)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                      u_furthestDepth)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearMinClampToEdge>), u_depthSampler)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                              u_skyProbe)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                                       u_rwCloudsDepth)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                               u_rwClouds)
DS_END();


static rdr::PipelineStateID CompileRenderVolumetricCloudsMainViewPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Atmosphere/VolumetricClouds/RenderVolumetricCloudsMainView.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "RenderVolumetricCloudsMainViewCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("RenderVolumetricCloudsMainViewPipeline"), shader);
}


static void RenderVolumetricCloudsMainView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, ViewRenderingSpec& viewSpec, const VolumetricCloudsParams& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "Volumetric Clouds (Main)");

	const RenderView& renderView = viewSpec.GetRenderView();

	const Uint32 renderedFrameIdx = renderView.GetRenderedFrameIdx();

	const math::Vector2u tracedPixel2x2 = math::Vector2u(renderedFrameIdx & 1u, (renderedFrameIdx >> 1u) & 1u);

	ShadingViewContext& shadingViewContext = viewSpec.GetShadingViewContext();

	const math::Vector2u cloudsResolution = renderer_params::fullResClouds ? viewSpec.GetRenderingRes() : math::Utils::DivideCeil(viewSpec.GetRenderingRes(), math::Vector2u(4u, 4u));

	SPT_CHECK(!!params.cloudscape);

	const rg::RGTextureViewHandle furthestDepth = graphBuilder.CreateTextureMipView(shadingViewContext.hiZ, 4u);

	VolumetricCloudsMainViewConstants shaderConstants;
	shaderConstants.resolution      = cloudsResolution;
	shaderConstants.rcpResolution   = cloudsResolution.cast<Real32>().cwiseInverse();
	shaderConstants.frameIdx        = renderedFrameIdx;
	shaderConstants.fullResTrace    = renderer_params::fullResClouds;
	shaderConstants.tracedPixel2x2  = tracedPixel2x2;

	const rg::RGTextureViewHandle cloudsTexture      = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Clouds"), rg::TextureDef(cloudsResolution, rhi::EFragmentFormat::RGBA16_S_Float));
	const rg::RGTextureViewHandle cloudsDepthTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Clouds Depth"), rg::TextureDef(cloudsResolution, rhi::EFragmentFormat::R32_S_Float));

	static const rdr::PipelineStateID pipeline = CompileRenderVolumetricCloudsMainViewPipeline();

	lib::MTHandle<RenderVolumetricCloudsMainViewDS> ds = graphBuilder.CreateDescriptorSet<RenderVolumetricCloudsMainViewDS>(RENDERER_RESOURCE_NAME("RenderVolumetricCloudsMainViewDS"));
	ds->u_passConstants = shaderConstants;
	ds->u_blueNoise256  = gfx::global::Resources::Get().blueNoise256.GetView();
	ds->u_furthestDepth = furthestDepth;
	ds->u_skyProbe      = shadingViewContext.skyProbe;
	ds->u_rwCloudsDepth = cloudsDepthTexture;
	ds->u_rwClouds      = cloudsTexture;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Volumetric Clouds Main View"),
						  pipeline,
						  math::Utils::DivideCeil(cloudsResolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds),
												 params.cloudscape->cloudscapeDS,
												 renderView.GetRenderViewDS()));

	if (renderer_params::fullResClouds)
	{
		shadingViewContext.volumetricClouds      = cloudsTexture;
		shadingViewContext.volumetricCloudsDepth = cloudsDepthTexture;
	}
	else
	{
		const math::Vector2u renderingRes = viewSpec.GetRenderingRes();

		const rg::RGTextureViewHandle cloudsTextureFullRes      = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Clouds Full Res"), rg::TextureDef(renderingRes, rhi::EFragmentFormat::RGBA16_S_Float));
		const rg::RGTextureViewHandle cloudsDepthTextureFullRes = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Clouds Depth Full Res"), rg::TextureDef(renderingRes, rhi::EFragmentFormat::R32_S_Float));

		if (params.accumulatedCloudsHistory.IsValid())
		{
			accumulate::AccumulateCloudsParams accumulateParams;
			accumulateParams.tracedClouds                  = cloudsTexture;
			accumulateParams.accumulatedCloudsHistory      = params.accumulatedCloudsHistory;
			accumulateParams.outAccumulatedClouds          = params.accumulatedClouds;
			accumulateParams.tracedCloudsDepth             = cloudsDepthTexture;
			accumulateParams.accumulatedCloudsDepthHistory = params.accumulatedCloudsDepthHistory;
			accumulateParams.outAccumulatedCloudsDepth     = params.accumulatedCloudsDepth;
			accumulateParams.accumulatedAge                = params.accumulatedCloudsAgeHistory;
			accumulateParams.outAge                        = params.accumulatedCloudsAge;
			accumulateParams.renderViewDS                  = renderView.GetRenderViewDS();
			accumulateParams.tracedPixel2x2                = tracedPixel2x2;

			accumulate::AccumulateClouds(graphBuilder, accumulateParams);
		}
		else
		{
			graphBuilder.BlitTexture(RG_DEBUG_NAME("Blit Accumulated Clouds"),
									 cloudsTexture,
									 params.accumulatedClouds,
									 rhi::ESamplerFilterType::Nearest);

			graphBuilder.BlitTexture(RG_DEBUG_NAME("Blit Accumulated Clouds Depth"),
									 cloudsDepthTexture,
									 params.accumulatedCloudsDepth,
									 rhi::ESamplerFilterType::Nearest);

			graphBuilder.ClearTexture(RG_DEBUG_NAME("Clear Accumulated Clouds Age"),
									  params.accumulatedCloudsAge,
									  rhi::ClearColor(0u, 0u, 0u, 0u));
		}

		upsample::UpsampleCloudsParams upsampleParams;
		upsampleParams.cloudsHalfRes      = params.accumulatedClouds;
		upsampleParams.cloudsDepthHalfRes = params.accumulatedCloudsDepth;
		upsampleParams.clouds             = cloudsTextureFullRes;
		upsampleParams.cloudsDepth        = cloudsDepthTextureFullRes;
		upsampleParams.frameIdx           = renderView.GetRenderedFrameIdx();

		upsample::UpsampleClouds(graphBuilder, upsampleParams);

		shadingViewContext.volumetricClouds      = cloudsTextureFullRes;
		shadingViewContext.volumetricCloudsDepth = cloudsDepthTextureFullRes;
	}
}

} // render_main_view

namespace sun_clouds_transmittance
{

struct CloudsTransmittanceMapParams
{
	const CloudscapeContext* cloudscape = nullptr;

	rg::RGTextureViewHandle cloudsTransmittanceTexture;

	math::Vector2u updateOffset = {};
	math::Vector2u updateSize   = {};
};


BEGIN_SHADER_STRUCT(RenderCloudsTransmittanceMapConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2u, updateOffset)
	SHADER_STRUCT_FIELD(math::Vector2f, rcpResolution)
	SHADER_STRUCT_FIELD(math::Matrix4f, viewProjectionMatrix)
	SHADER_STRUCT_FIELD(math::Matrix4f, invViewProjectionMatrix)
	SHADER_STRUCT_FIELD(math::Vector3f, direction)
END_SHADER_STRUCT();


DS_BEGIN(RenderCloudsTransmittanceMapDS, rg::RGDescriptorSetState<RenderCloudsTransmittanceMapDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<RenderCloudsTransmittanceMapConstants>), u_constants)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),                           u_rwTransmittanceMap)
DS_END();


static rdr::PipelineStateID CompileRenderCloudsTransmittanceMapPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Atmosphere/VolumetricClouds/RenderCloudsTransmittanceMap.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "RenderCloudsTransmittanceMapCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("RenderCloudsTransmittanceMapPipeline"), shader);
}


static CloudsTransmittanceMap RenderCloudsTransmittanceMap(rg::RenderGraphBuilder& graphBuilder, const CloudsTransmittanceMapParams& params)
{
	const math::Vector2u resolution = params.cloudsTransmittanceTexture->GetResolution2D();

	static const rdr::PipelineStateID pipeline = CompileRenderCloudsTransmittanceMapPipeline();

	const AtmosphereContext& atmosphere = params.cloudscape->atmosphere;
	const DirectionalLightGPUData& mainDirLight = *atmosphere.mainDirectionalLight;

	const CloudscapeConstants& cloudscapeConstants = params.cloudscape->cloudscapeConstants;

	RenderCloudsTransmittanceMapConstants shaderConstants;
	shaderConstants.resolution    = resolution;
	shaderConstants.updateOffset  = params.updateOffset;
	shaderConstants.rcpResolution = resolution.cast<Real32>().cwiseInverse();
	shaderConstants.direction     = -mainDirLight.direction;

	math::Vector3f center = params.cloudscape->cloudscapeConstants.cloudsAtmosphereCenter;
	center.z() = 0.f;

	BuildCloudsTransmittanceMapMatrices(center, mainDirLight.direction, cloudscapeConstants.cloudscapeRange, cloudscapeConstants.cloudscapeOuterHeight, OUT shaderConstants.viewProjectionMatrix);
	shaderConstants.invViewProjectionMatrix = shaderConstants.viewProjectionMatrix.inverse();

	lib::MTHandle<RenderCloudsTransmittanceMapDS> ds = graphBuilder.CreateDescriptorSet<RenderCloudsTransmittanceMapDS>(RENDERER_RESOURCE_NAME("RenderCloudsTransmittanceMapDS"));
	ds->u_rwTransmittanceMap = params.cloudsTransmittanceTexture;
	ds->u_constants          = shaderConstants;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Clouds Sun Transmittance Map"),
						  pipeline,
						  math::Utils::DivideCeil(params.updateSize, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(ds, params.cloudscape->cloudscapeDS));

	CloudsTransmittanceMap cloudsTransmittanceMap
	{
		.cloudsTransmittanceTexture = params.cloudsTransmittanceTexture,
		.viewProjectionMatrix       = shaderConstants.viewProjectionMatrix
	};

	return cloudsTransmittanceMap;
}

} // sun_clouds_transmittance

namespace cloudscape_probe
{

static constexpr Uint32 maxProbesToUpdate = 2u;


struct ProbesUpdateParams
{
	const CloudscapeContext* cloudscape;

	rg::RGTextureViewHandle skyProbe;

	rg::RGTextureViewHandle cloduscapeProbesTexture;
	rg::RGTextureViewHandle cloduscapeSimpleProbesTexture;

	Uint32 tracesPerProbe = 1024u;

	lib::StaticArray<math::Vector4u, maxProbesToUpdate> probeCoords;
	Bool forceFullUpdate = false; // if true, will update all probes, otherwise will only update the probes that are in probeCoords
};


BEGIN_SHADER_STRUCT(UpdateCloudscapeProbesConstants)
	SHADER_STRUCT_FIELD(Uint32,                                                              raysPerProbe)
	SHADER_STRUCT_FIELD(math::Vector2u,                                                      compressedProbeDataRes)
	SHADER_STRUCT_FIELD(SPT_SINGLE_ARG(lib::StaticArray<math::Vector4u, maxProbesToUpdate>), probesToUpdate)
END_SHADER_STRUCT();


namespace trace
{

DS_BEGIN(TraceCloudscapeProbesDS, rg::RGDescriptorSetState<TraceCloudscapeProbesDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<UpdateCloudscapeProbesConstants>), u_constants)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                    u_skyViewProbe)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                     u_rwTraceResult)
DS_END();


static rdr::PipelineStateID CompileTraceCloudscapeProbesPipeline(Bool fullUpdate)
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("FULL_UPDATE", fullUpdate ? "1" : "0"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Atmosphere/VolumetricClouds/TraceCloudscapeProbes.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "TraceCloudscapeProbesCS"), compilationSettings);
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("TraceCloudscapeProbesPipeline"), shader);
}


rg::RGTextureViewHandle TraceCloudscapeProbes(rg::RenderGraphBuilder& graphBuilder, const ProbesUpdateParams& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u probesNum = params.cloudscape->cloudscapeConstants.probesNum;

	const Uint32 updatedProbesNum = params.forceFullUpdate ? (probesNum.x() * probesNum.y()) : static_cast<Uint32>(params.probeCoords.size());

	const math::Vector2u traceResultRes = math::Vector2u(params.tracesPerProbe, updatedProbesNum);

	const rg::RGTextureViewHandle traceResult = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Cloudscape Probes Trace"), rg::TextureDef(traceResultRes, rhi::EFragmentFormat::RGBA16_S_Float));

	const rdr::PipelineStateID pipeline = CompileTraceCloudscapeProbesPipeline(params.forceFullUpdate);

	UpdateCloudscapeProbesConstants shaderConstants;
	shaderConstants.raysPerProbe           = params.tracesPerProbe;
	shaderConstants.compressedProbeDataRes = params.cloudscape->cloudscapeConstants.pixelsPerProbe;
	shaderConstants.probesToUpdate         = params.probeCoords;

	lib::MTHandle<TraceCloudscapeProbesDS> ds = graphBuilder.CreateDescriptorSet<TraceCloudscapeProbesDS>(RENDERER_RESOURCE_NAME("TraceCloudscapeProbesDS"));
	ds->u_rwTraceResult = traceResult;
	ds->u_skyViewProbe  = params.skyProbe;
	ds->u_constants     = shaderConstants;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Trace Cloudscape Probes"),
						  pipeline,
						  traceResultRes,
						  rg::BindDescriptorSets(std::move(ds),
												 params.cloudscape->cloudscapeDS));

	return traceResult;
}

} // trace

namespace compress
{

DS_BEGIN(CompressCloudscapeProbesDS, rg::RGDescriptorSetState<CompressCloudscapeProbesDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<UpdateCloudscapeProbesConstants>), u_constants)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                    u_traceResult)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                     u_rwCompressedProbes)
DS_END();


static rdr::PipelineStateID CompileCompressCloudscapeProbesPipeline(Bool fullUpdate)
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("FULL_UPDATE", fullUpdate ? "1" : "0"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Atmosphere/VolumetricClouds/CompressCloudscapeProbes.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CompressCloudscapeProbesCS"), compilationSettings);
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("CompressCloudscapeProbesPipeline"), shader);
}


void CompressCloudscapeProbes(rg::RenderGraphBuilder& graphBuilder, const ProbesUpdateParams& params, rg::RGTextureViewHandle traceResult)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u probesNum = params.cloudscape->cloudscapeConstants.probesNum;

	const Uint32 updatedProbesNum = params.forceFullUpdate ? (probesNum.x() * probesNum.y()) : static_cast<Uint32>(params.probeCoords.size());

	const rdr::PipelineStateID pipeline = CompileCompressCloudscapeProbesPipeline(params.forceFullUpdate);

	UpdateCloudscapeProbesConstants shaderConstants;
	shaderConstants.raysPerProbe           = params.tracesPerProbe;
	shaderConstants.compressedProbeDataRes = params.cloudscape->cloudscapeConstants.pixelsPerProbe;
	shaderConstants.probesToUpdate         = params.probeCoords;

	lib::MTHandle<CompressCloudscapeProbesDS> ds = graphBuilder.CreateDescriptorSet<CompressCloudscapeProbesDS>(RENDERER_RESOURCE_NAME("CompressCloudscapeProbesDS"));
	ds->u_rwCompressedProbes       = params.cloduscapeProbesTexture;
	ds->u_traceResult              = traceResult;
	ds->u_constants                = shaderConstants;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Compress Cloudscape Probes"),
						  pipeline,
						  math::Vector2u(1u, updatedProbesNum),
						  rg::BindDescriptorSets(std::move(ds),
												 params.cloudscape->cloudscapeDS));
}

} // compress


void UpdateCloudscapeProbes(rg::RenderGraphBuilder& graphBuilder, const ProbesUpdateParams& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!params.skyProbe);
	SPT_CHECK(!!params.cloduscapeProbesTexture);

	const rg::RGTextureViewHandle traceResult = trace::TraceCloudscapeProbes(graphBuilder, params);

	compress::CompressCloudscapeProbes(graphBuilder, params, traceResult);
}

namespace high_res
{

BEGIN_SHADER_STRUCT(UpdateCloudscapeHighResProbeConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, updateOffset)
	SHADER_STRUCT_FIELD(Uint32,         updateLoopIdx)
END_SHADER_STRUCT();


DS_BEGIN(UpdateCloudscapeHighResProbeDS, rg::RGDescriptorSetState<UpdateCloudscapeHighResProbeDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<UpdateCloudscapeHighResProbeConstants>), u_constants)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                          u_skyViewProbe)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                  u_blueNoise256)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                           u_rwProbe)
DS_END();


static rdr::PipelineStateID CompileUpdateCloudscapeHighResProbePipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Atmosphere/VolumetricClouds/UpdateCloudscapeHighResProbe.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "UpdateCloudscapeHighResProbeCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("UpdateCloudscapeHighResProbePipeline"), shader);
}


struct UpdateCloudscapeHighResProbeParams
{
	const CloudscapeContext* cloudscape = nullptr;
	rg::RGTextureViewHandle skyProbe;
	rg::RGTextureViewHandle cloudscapeProbe;

	math::Vector2u updateOffset = {};
	math::Vector2u updateSize   = {};

	Uint32 updateLoopIdx = 0u;
};


void UpdateCloudscapeHighResProbe(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, const UpdateCloudscapeHighResProbeParams& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.cloudscapeProbe->GetResolution2D();

	static const rdr::PipelineStateID pipeline = CompileUpdateCloudscapeHighResProbePipeline();

	UpdateCloudscapeHighResProbeConstants shaderConstants;
	shaderConstants.updateOffset  = params.updateOffset;
	shaderConstants.updateLoopIdx = params.updateLoopIdx;

	lib::MTHandle<UpdateCloudscapeHighResProbeDS> ds = graphBuilder.CreateDescriptorSet<UpdateCloudscapeHighResProbeDS>(RENDERER_RESOURCE_NAME("UpdateCloudscapeHighResProbeDS"));
	ds->u_constants    = shaderConstants;
	ds->u_skyViewProbe = params.skyProbe;
	ds->u_blueNoise256 = gfx::global::Resources::Get().blueNoise256.GetView();
	ds->u_rwProbe      = params.cloudscapeProbe;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Update High Res Probe"),
						  pipeline,
						  params.updateSize,
						  rg::BindDescriptorSets(std::move(ds),
												 viewSpec.GetRenderView().GetRenderViewDS(),
												 params.cloudscape->cloudscapeDS));
}

} // high_res

} // cloudscape_probe

VolumetricCloudsRenderer::VolumetricCloudsRenderer()
	: m_mainViewClouds(RENDERER_RESOURCE_NAME("Main View Clouds"))
	, m_mainViewCloudsDepth(RENDERER_RESOURCE_NAME("Main View Clouds Depth"))
	, m_mainViewCloudsAge(RENDERER_RESOURCE_NAME("Main View Clouds Age"))
{
	const Real32 volumetricCloudsMinHeight = 256.f;
	const Real32 volumetricCloudsMaxHeight = 2048.f;
	const Real32 volumetricCloudsRange     = 8000.f;

	const Real32 radius = (volumetricCloudsRange * volumetricCloudsRange + volumetricCloudsMinHeight * volumetricCloudsMinHeight) / (2.f * volumetricCloudsMinHeight);

	m_cloudscapeConstants.cloudsAtmosphereCenterZ     = volumetricCloudsMinHeight - radius;
	m_cloudscapeConstants.cloudsAtmosphereInnerRadius = radius;
	m_cloudscapeConstants.cloudsAtmosphereOuterRadius = radius + (volumetricCloudsMaxHeight - volumetricCloudsMinHeight);
	m_cloudscapeConstants.cloudscapeRange             = volumetricCloudsRange;
	m_cloudscapeConstants.cloudscapeInnerHeight       = volumetricCloudsMinHeight;
	m_cloudscapeConstants.cloudscapeOuterHeight       = volumetricCloudsMaxHeight;

	m_cloudscapeConstants.probesNum      = math::Vector2u::Constant(32u);
	m_cloudscapeConstants.pixelsPerProbe = math::Vector2u::Constant(32u);

	const math::Vector2u probesTextureRes = m_cloudscapeConstants.pixelsPerProbe.cwiseProduct(m_cloudscapeConstants.probesNum);

	m_cloudscapeConstants.probesSpacing        = math::Vector2f(256.f, 256.f);
	m_cloudscapeConstants.probesHeight         = 50.f;
	m_cloudscapeConstants.probesOrigin         = -m_cloudscapeConstants.probesNum.cast<Real32>().cwiseProduct(m_cloudscapeConstants.probesSpacing * 0.5f);
	m_cloudscapeConstants.rcpProbesSpacing     = m_cloudscapeConstants.probesSpacing.cwiseInverse();
	m_cloudscapeConstants.uvPerProbe           = m_cloudscapeConstants.probesNum.cast<Real32>().cwiseInverse();
	m_cloudscapeConstants.uvPerProbeNoBorders  = (m_cloudscapeConstants.pixelsPerProbe - math::Vector2u::Constant(1u)).cast<Real32>().cwiseQuotient(probesTextureRes.cast<Real32>());
	m_cloudscapeConstants.uvBorder             = probesTextureRes.cast<Real32>().cwiseInverse();
	m_cloudscapeConstants.highResProbeRes      = math::Vector2u(512u, 512u);
	m_cloudscapeConstants.highResProbeRcpRes   = m_cloudscapeConstants.highResProbeRes.cast<Real32>().cwiseInverse();

	InitTextures();
}

void VolumetricCloudsRenderer::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings)
{
	m_volumetricCloudsEnabled = renderer_params::enableVolumetricClouds;

	if (m_volumetricCloudsEnabled)
	{
		const CloudscapeContext cloudscapeContext = CreateFrameCloudscapeContext(graphBuilder, renderScene, viewSpecs, settings);

		const math::Vector2u updateTileSize = math::Vector2u(256u, 256u);

		sun_clouds_transmittance::CloudsTransmittanceMapParams cloudsTransmittanceParams;
		cloudsTransmittanceParams.cloudscape                 = &cloudscapeContext;
		cloudsTransmittanceParams.cloudsTransmittanceTexture = graphBuilder.AcquireExternalTextureView(m_cloudscapeTransmittance);

		if (cloudscapeContext.resetAccumulation || renderer_params::forceTransmittanceMapFullUpdates)
		{
			cloudsTransmittanceParams.updateOffset = math::Vector2u::Zero();
			cloudsTransmittanceParams.updateSize   = cloudsTransmittanceParams.cloudsTransmittanceTexture->GetResolution2D();
		}
		else
		{
			const math::Vector2u tilesNum = math::Utils::DivideCeil(cloudsTransmittanceParams.cloudsTransmittanceTexture->GetResolution2D(), updateTileSize);

			const Uint32 tileIdx = cloudscapeContext.frameIdx % (tilesNum.x() * tilesNum.y());

			const math::Vector2u tileCoords = math::Vector2u(tileIdx % tilesNum.x(), tileIdx / tilesNum.x());

			cloudsTransmittanceParams.updateOffset = tileCoords.cwiseProduct(updateTileSize);
			cloudsTransmittanceParams.updateSize   = updateTileSize;
		}

		m_cloudsTransmittanceMap = sun_clouds_transmittance::RenderCloudsTransmittanceMap(graphBuilder, cloudsTransmittanceParams);

		for (ViewRenderingSpec* viewSpec : viewSpecs)
		{
			SPT_CHECK(!!viewSpec);
			RenderPerView(graphBuilder, renderScene, *viewSpec, cloudscapeContext);
		}
	}
}

void VolumetricCloudsRenderer::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const CloudscapeContext& cloudscapeContext)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!m_baseShapeNoiseTexture);

	viewSpec.GetRenderViewEntry(RenderViewEntryDelegates::VolumetricClouds).AddRawMember(this, &VolumetricCloudsRenderer::RenderVolumetricClouds, cloudscapeContext);

	if (m_enqueuedCloudscapeProbesGlobalUpdate || cloudscapeContext.resetAccumulation || renderer_params::forceLRCloudsProbeFullUpdates)
	{
		UpdateAllCloudscapeProbes(graphBuilder, renderScene, viewSpec, cloudscapeContext);
		m_enqueuedCloudscapeProbesGlobalUpdate = false;
	}
	else
	{
		UpdateCloudscapeProbesPerFrame(graphBuilder, renderScene, viewSpec, cloudscapeContext);
	}

	const rg::RGTextureViewHandle cloudscapeProbes       = graphBuilder.TryAcquireExternalTextureView(m_cloudscapeProbes);
	const rg::RGTextureViewHandle cloudscapeHighResProbe = graphBuilder.TryAcquireExternalTextureView(m_cloudscapeHighResProbe);

	cloudscape_probe::high_res::UpdateCloudscapeHighResProbeParams highResProbeParams;
	highResProbeParams.cloudscape      = &cloudscapeContext;
	highResProbeParams.skyProbe        = viewSpec.GetShadingViewContext().skyProbe;
	highResProbeParams.cloudscapeProbe = cloudscapeHighResProbe;

	if (cloudscapeContext.resetAccumulation || renderer_params::forceHRCloudsProbeFullUpdates)
	{
		highResProbeParams.updateOffset  = math::Vector2u::Zero();
		highResProbeParams.updateSize    = cloudscapeHighResProbe->GetResolution2D();
		highResProbeParams.updateLoopIdx = cloudscapeContext.frameIdx;
	}
	else
	{
		const math::Vector2u highResProbeTileSize = math::Vector2u::Constant(128u);

		const math::Vector2u tilesNum = math::Utils::DivideCeil(cloudscapeHighResProbe->GetResolution2D(), highResProbeTileSize);
		const Uint32 tileIdx = cloudscapeContext.frameIdx % (tilesNum.x() * tilesNum.y());
		const math::Vector2u tileCoords = math::Vector2u(tileIdx % tilesNum.x(), tileIdx / tilesNum.x());

		highResProbeParams.updateLoopIdx   = cloudscapeContext.frameIdx / (tilesNum.x() * tilesNum.y());
		highResProbeParams.updateOffset    = tileCoords.cwiseProduct(highResProbeTileSize);
		highResProbeParams.updateSize      = highResProbeTileSize;
	}

	cloudscape_probe::high_res::UpdateCloudscapeHighResProbe(graphBuilder, viewSpec, highResProbeParams);

	ShadingViewContext& shadingViewContext = viewSpec.GetShadingViewContext();

	lib::MTHandle<CloudscapeProbesDS> cloudscapeFullDS = graphBuilder.CreateDescriptorSet<CloudscapeProbesDS>(RENDERER_RESOURCE_NAME("CloudscapeProbesDS"));
	cloudscapeFullDS->u_cloudscapeConstants    = cloudscapeContext.cloudscapeConstants;
	cloudscapeFullDS->u_cloudscapeProbes       = cloudscapeProbes;
	cloudscapeFullDS->u_cloudscapeHighResProbe = cloudscapeHighResProbe;

	shadingViewContext.cloudscapeProbesDS = std::move(cloudscapeFullDS);
}

void VolumetricCloudsRenderer::RenderVolumetricClouds(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context, const CloudscapeContext cloudscapeContext)
{
	SPT_PROFILER_FUNCTION();

	m_mainViewClouds.Update(viewSpec.GetRenderingHalfRes());
	m_mainViewCloudsDepth.Update(viewSpec.GetRenderingHalfRes());
	m_mainViewCloudsAge.Update(viewSpec.GetRenderingHalfRes());

	render_main_view::VolumetricCloudsParams cloudsMainViewParams;
	cloudsMainViewParams.cloudscape = &cloudscapeContext;

	cloudsMainViewParams.accumulatedClouds      = graphBuilder.AcquireExternalTextureView(m_mainViewClouds.GetCurrent());
	cloudsMainViewParams.accumulatedCloudsDepth = graphBuilder.AcquireExternalTextureView(m_mainViewCloudsDepth.GetCurrent());
	cloudsMainViewParams.accumulatedCloudsAge   = graphBuilder.AcquireExternalTextureView(m_mainViewCloudsAge.GetCurrent());

	if (m_mainViewClouds.HasHistory())
	{
		cloudsMainViewParams.accumulatedCloudsHistory      = graphBuilder.AcquireExternalTextureView(m_mainViewClouds.GetHistory());
		cloudsMainViewParams.accumulatedCloudsDepthHistory = graphBuilder.AcquireExternalTextureView(m_mainViewCloudsDepth.GetHistory());
		cloudsMainViewParams.accumulatedCloudsAgeHistory   = graphBuilder.AcquireExternalTextureView(m_mainViewCloudsAge.GetHistory());
	}

	render_main_view::RenderVolumetricCloudsMainView(graphBuilder, scene, viewSpec, cloudsMainViewParams);
}

CloudscapeContext VolumetricCloudsRenderer::CreateFrameCloudscapeContext(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings)
{
	SPT_CHECK(!viewSpecs.empty());

	const ViewRenderingSpec* mainView = viewSpecs[0];

	const AtmosphereSceneSubsystem& atmosphereSubsystem = renderScene.GetSceneSubsystemChecked<AtmosphereSceneSubsystem>();

	const AtmosphereContext& atmosphere = atmosphereSubsystem.GetAtmosphereContext();

	const RenderView& renderView = mainView->GetRenderView();
	const math::Vector3f viewLocation = renderView.GetLocation();

	const math::Vector3f cloudsAtmosphereCenter = math::Vector3f(viewLocation.x(), viewLocation.y(), m_cloudscapeConstants.cloudsAtmosphereCenterZ);

	m_cloudscapeConstants.baseShapeNoiseScale          = 1.f / renderer_params::baseShapeNoiseMeters;
	m_cloudscapeConstants.detailShapeNoiseStrength0    = renderer_params::detailShapeNoiseStrength0;
	m_cloudscapeConstants.detailShapeNoiseScale0       = 1.f / renderer_params::detailShapeNoiseMeters0;
	m_cloudscapeConstants.detailShapeNoiseStrength1    = renderer_params::detailShapeNoiseStrength1;
	m_cloudscapeConstants.detailShapeNoiseScale1       = 1.f / renderer_params::detailShapeNoiseMeters1;
	m_cloudscapeConstants.curlNoiseScale               = 1.f / renderer_params::curlNoiseMeters;
	m_cloudscapeConstants.curlMaxoffset                = renderer_params::curlNoiseOffset;
	m_cloudscapeConstants.weatherMapScale              = 1.f / renderer_params::weatherMapMeters;
	m_cloudscapeConstants.cloudsAtmosphereCenter       = cloudsAtmosphereCenter;
	m_cloudscapeConstants.globalDensity                = renderer_params::globalDensity;
	m_cloudscapeConstants.globalCoverageOffset         = renderer_params::globalCoverageOffset;
	m_cloudscapeConstants.globalCloudsHeightOffset     = renderer_params::cloudsHeightOffset;
	m_cloudscapeConstants.globalCoverageMultiplier     = renderer_params::globalCoverageMultiplier;
	m_cloudscapeConstants.globalCloudsHeightMultiplier = renderer_params::cloudsHeightMultiplier;
	m_cloudscapeConstants.time                         = renderScene.GetCurrentFrameRef().GetTime();
	m_cloudscapeConstants.mainDirectionalLight         = *atmosphere.mainDirectionalLight;

	lib::MTHandle<CloudscapeDS> ds = graphBuilder.CreateDescriptorSet<CloudscapeDS>(RENDERER_RESOURCE_NAME("CloudscapeDS"));
	ds->u_cloudscapeConstants = m_cloudscapeConstants;
	ds->u_atmosphereConstants = atmosphere.atmosphereParamsBuffer->CreateFullView();
	ds->u_transmittanceLUT    = atmosphere.transmittanceLUT;
	ds->u_baseShapeNoise      = graphBuilder.AcquireExternalTextureView(m_baseShapeNoiseTexture);
	ds->u_detailShapeNoise    = graphBuilder.AcquireExternalTextureView(m_detailShapeNoiseTexture);
	ds->u_curlNoise           = graphBuilder.AcquireExternalTextureView(m_curlNoise);
	ds->u_weatherMap          = graphBuilder.AcquireExternalTextureView(m_weatherMap);
	ds->u_densityLUT          = graphBuilder.AcquireExternalTextureView(m_densityLUT);

	const Bool resetAccumulation = settings.resetAccumulation;

	CloudscapeContext context{ atmosphere, m_cloudscapeConstants, ds };
	context.resetAccumulation = resetAccumulation;
	context.frameIdx          = renderView.GetRenderedFrameIdx();

	return context;
}

void VolumetricCloudsRenderer::UpdateAllCloudscapeProbes(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const CloudscapeContext& cloudscapeContext)
{
	SPT_PROFILER_FUNCTION();

	UpdateCloudscapeProbes(graphBuilder, renderScene, viewSpec, cloudscapeContext, 256u, lib::Span<const math::Vector2u>{});
}

void VolumetricCloudsRenderer::UpdateCloudscapeProbesPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const CloudscapeContext& cloudscapeContext)
{
	lib::StaticArray<math::Vector2u, cloudscape_probe::maxProbesToUpdate> probeCoords;

	const Uint32 frameIdx = viewSpec.GetRenderView().GetRenderedFrameIdx();
	const Uint32 xOffset = (frameIdx & 15u) * cloudscape_probe::maxProbesToUpdate;
	const Uint32 yOffset = ((frameIdx / 16u) & 31u);

	for (Uint32 i = 0u; i < cloudscape_probe::maxProbesToUpdate; ++i)
	{
		probeCoords[i] = math::Vector2u(xOffset + i, yOffset);
	}

	UpdateCloudscapeProbes(graphBuilder, renderScene, viewSpec, cloudscapeContext, 256u, probeCoords);
}

void VolumetricCloudsRenderer::UpdateCloudscapeProbes(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const CloudscapeContext& cloudscapeContext, Uint32 raysNumPerProbe, lib::Span<const math::Vector2u> probeCoords)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(probeCoords.size() <= cloudscape_probe::maxProbesToUpdate);

	const rg::RGTextureViewHandle cloudscapeProbes = graphBuilder.AcquireExternalTextureView(m_cloudscapeProbes);

	ShadingViewContext& shadingViewContext = viewSpec.GetShadingViewContext();

	cloudscape_probe::ProbesUpdateParams updateParams;
	updateParams.cloudscape                    = &cloudscapeContext;
	updateParams.cloduscapeProbesTexture       = cloudscapeProbes;
	updateParams.tracesPerProbe                = raysNumPerProbe;
	updateParams.skyProbe                      = shadingViewContext.skyProbe;

	if (probeCoords.empty())
	{
		updateParams.forceFullUpdate = true;
	}
	else
	{
		updateParams.forceFullUpdate = false;

		for (Uint32 i = 0u; i < probeCoords.size(); ++i)
		{
			updateParams.probeCoords[i] = math::Vector4u(probeCoords[i].x(), probeCoords[i].y(), 0u, 0u);
		}
	}

	cloudscape_probe::UpdateCloudscapeProbes(graphBuilder, updateParams);
}

void VolumetricCloudsRenderer::InitTextures()
{
	LoadOrCreateBaseShapeNoiseTexture();
	LoadOrCreateDetailShapeNoiseTexture();
	LoadWeatherMapTexture();
	LoadCurlNoiseTexture();
	LoadDensityLUTTexture();

	const math::Vector2u probeRes  = math::Vector2u(32u, 32u);
	const math::Vector2u probesRes = math::Vector2u(32u, 32u);

	rhi::TextureDefinition cloudscapeProbesDef;
	cloudscapeProbesDef.resolution = math::Vector2u(probesRes.cwiseProduct(probeRes));
	cloudscapeProbesDef.format     = rhi::EFragmentFormat::RGBA16_S_Float;
	cloudscapeProbesDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferDest);
	m_cloudscapeProbes = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("Cloudscape Probes"), cloudscapeProbesDef, rhi::EMemoryUsage::GPUOnly);

	rhi::TextureDefinition cloudscapeHighResProbeDef;
	cloudscapeHighResProbeDef.resolution = m_cloudscapeConstants.highResProbeRes;
	cloudscapeHighResProbeDef.format     = rhi::EFragmentFormat::RGBA16_S_Float;
	cloudscapeHighResProbeDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferDest);
	m_cloudscapeHighResProbe = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("Cloudscape High Res Probe"), cloudscapeHighResProbeDef, rhi::EMemoryUsage::GPUOnly);

	rhi::TextureDefinition cloudscapeTransmittanceDef;
	cloudscapeTransmittanceDef.resolution = math::Vector2u(2048u, 2048u);
	cloudscapeTransmittanceDef.format = rhi::EFragmentFormat::R16_UN_Float;
	cloudscapeTransmittanceDef.usage = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferDest);
	m_cloudscapeTransmittance = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("Cloudscape Transmittance Texture"), cloudscapeTransmittanceDef, rhi::EMemoryUsage::GPUOnly);

	rhi::TextureDefinition mainViewCloudsDef;
	mainViewCloudsDef.type = rhi::ETextureType::Texture2D;
	mainViewCloudsDef.format =  rhi::EFragmentFormat::RGBA16_S_Float;
	mainViewCloudsDef.usage = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferDest);
	m_mainViewClouds.SetDefinition(mainViewCloudsDef);

	rhi::TextureDefinition mainViewCloudsDepthDef;
	mainViewCloudsDepthDef.type = rhi::ETextureType::Texture2D;
	mainViewCloudsDepthDef.format = rhi::EFragmentFormat::R32_S_Float;
	mainViewCloudsDepthDef.usage = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferDest);
	m_mainViewCloudsDepth.SetDefinition(mainViewCloudsDepthDef);

	rhi::TextureDefinition mainViewCloudsAgeDef;
	mainViewCloudsAgeDef.type   = rhi::ETextureType::Texture2D;
	mainViewCloudsAgeDef.format = rhi::EFragmentFormat::R8_U_Int;
	mainViewCloudsAgeDef.usage  = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferDest);
	m_mainViewCloudsAge.SetDefinition(mainViewCloudsAgeDef);
}

void VolumetricCloudsRenderer::LoadOrCreateBaseShapeNoiseTexture()
{
	const lib::String texturePath = engn::Paths::GetContentPath() + "/RenderingPipeline/Textures/Clouds/BaseShapeNoise.dds";
	lib::SharedPtr<rdr::Texture> texture = gfx::TextureLoader::LoadTexture(texturePath);
	if (!texture)
	{
		SPT_LOG_TRACE(VolumetricCloudsRenderer, "Failed to load base shape noise texture from path: {}\nGenerating new texture", texturePath);

		const CloudsNoiseData baseShapeNoiseData = ComputeBaseShapeNoiseTextureWorley();

		SPT_MAYBE_UNUSED
		const Bool saveResult = gfx::TextureWriter::SaveTexture(baseShapeNoiseData.resolution, baseShapeNoiseData.format, baseShapeNoiseData.linearData, texturePath);
		SPT_CHECK(saveResult);

		SPT_CHECK(!!texture);
	}

	m_baseShapeNoiseTexture = texture->CreateView(RENDERER_RESOURCE_NAME("Volumetric Clouds Base Shape Noise"));
}

void VolumetricCloudsRenderer::LoadOrCreateDetailShapeNoiseTexture()
{
	const lib::String texturePath = engn::Paths::GetContentPath() + "/RenderingPipeline/Textures/Clouds/VoxelCloud.dds";
	lib::SharedPtr<rdr::Texture> texture = gfx::TextureLoader::LoadTexture(texturePath);

	if (!texture)
	{
		SPT_LOG_TRACE(VolumetricCloudsRenderer, "Failed to load detail shape noise texture from path: {}\nGenerating new texture", texturePath);

		const CloudsNoiseData baseShapeNoiseData = ComputeDetailShapeNoiseTextureWorley();

		SPT_MAYBE_UNUSED
		const Bool saveResult = gfx::TextureWriter::SaveTexture(baseShapeNoiseData.resolution, baseShapeNoiseData.format, baseShapeNoiseData.linearData, texturePath);
		SPT_CHECK(saveResult);

		texture = gfx::TextureLoader::LoadTexture(texturePath);
		SPT_CHECK(!!texture);
	}

	m_detailShapeNoiseTexture = texture->CreateView(RENDERER_RESOURCE_NAME("Volumetric Clouds Detail Shape Noise"));
}

void VolumetricCloudsRenderer::LoadWeatherMapTexture()
{
	const lib::String texturePath = engn::Paths::GetContentPath() + "/RenderingPipeline/Textures/Clouds/WeatherMap.dds";

	lib::SharedPtr<rdr::Texture> texture = gfx::TextureLoader::LoadTexture(texturePath);
	SPT_CHECK(!!texture);

	m_weatherMap = texture->CreateView(RENDERER_RESOURCE_NAME("Weather Map"));
}
 
void VolumetricCloudsRenderer::LoadCurlNoiseTexture()
{
	const lib::String texturePath = engn::Paths::GetContentPath() + "/RenderingPipeline/Textures/Clouds/CurlNoise.dds";

	lib::SharedPtr<rdr::Texture> texture = gfx::TextureLoader::LoadTexture(texturePath);
	SPT_CHECK(!!texture);

	m_curlNoise = texture->CreateView(RENDERER_RESOURCE_NAME("Curl Noise"));
}

void VolumetricCloudsRenderer::LoadDensityLUTTexture()
{
	const lib::String texturePath = engn::Paths::GetContentPath() + "/RenderingPipeline/Textures/Clouds/DensityLUT.png";
	lib::SharedPtr<rdr::Texture> texture = gfx::TextureLoader::LoadTexture(texturePath);
	SPT_CHECK(!!texture);
	m_densityLUT = texture->CreateView(RENDERER_RESOURCE_NAME("Density LUT"));
}

} // spt::rsc::clouds
