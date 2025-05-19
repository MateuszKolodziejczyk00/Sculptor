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

namespace params
{

RendererFloatParameter baseShapeNoiseMeters("Base Shape Noise Meters", { "Clouds" }, 50000.f, 0.f, 60000.f);
RendererFloatParameter detailShapeNoiseMeters("Detail Shape Noise Meters", { "Clouds" }, 3400.f, 0.f, 20000.f);

RendererFloatParameter globalDensity("Global Density", { "Clouds" }, 1.f, 0.f, 5.f);
RendererFloatParameter globalCoverage("Global Coverage", { "Clouds" }, 0.53f, 0.f, 1.f);

RendererBoolParameter enableVolumetricClouds("Enable Volumetric Clouds", { "Clouds" }, false);

} // params


static void BuildCloudsTransmittanceMapMatrices(const math::Vector3f& lightDirection, Real32 cloudscapeRange, Real32 cloudscapeMaxHeight, math::Matrix4f& outViewProj)
{
	const math::Vector3f center = math::Vector3f(0.f, 0.f, 0.f);

	math::ParametrizedLine<Real32, 3> line(center, lightDirection);

	const math::Vector3f edges[5] =
	{
		center + math::Vector3f( cloudscapeRange,  cloudscapeRange, 0.f),
		center + math::Vector3f( cloudscapeRange, -cloudscapeRange, 0.f),
		center + math::Vector3f(-cloudscapeRange,  cloudscapeRange, 0.f),
		center + math::Vector3f(-cloudscapeRange, -cloudscapeRange, 0.f),
		center + math::Vector3f(0.f, 0.f, cloudscapeMaxHeight)
	};

	Real32 maxDistance = 0.f;
	for (const auto& edge : edges)
	{
		const Real32 distance = line.distance(edge);
		maxDistance = std::max(maxDistance, distance);
	}

	// planes don't matter, it won't be used for culling, 
	const Real32 nearPlane = 0.0001f;
	const Real32 farPlane  = 1.f;

	const math::Matrix4f proj = projection::CreateOrthographicsProjection(nearPlane, farPlane, -maxDistance, maxDistance, -maxDistance, maxDistance);

	const math::Quaternion rotation = math::Quaternionf::FromTwoVectors(math::Vector3f(1.f, 0.f, 0.f), lightDirection);

	const math::Matrix3f rotationMatrix = rotation.toRotationMatrix();

	math::Matrix4f view = math::Matrix4f::Zero();
	view.topLeftCorner<3, 3>() = rotationMatrix.inverse();
	view.topRightCorner<3, 1>() = -center;
	view(3, 3) = 1.f;

	outViewProj = proj * view;
}


namespace render_sky_probe
{

DS_BEGIN(RenderSkyProbeDS, rg::RGDescriptorSetState<RenderSkyProbeDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<AtmosphereParams>),                    u_atmosphereParams)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_skyViewLUT)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),                            u_rwProbe)
DS_END();


static rdr::PipelineStateID CompileRenderSkyProbePipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Atmosphere/VolumetricClouds/RenderSkyProbe.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "RenderSkyProbeCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("RenderSkyProbePipeline"), shader);
}


static rg::RGTextureViewHandle RenderSkyProbe(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, ViewRenderingSpec& viewSpec, const AtmosphereContext& atmosphere, rg::RGTextureViewHandle skyViewLUT)
{
	SPT_PROFILER_FUNCTION();

	rg::TextureDef skyViewProbeDef(math::Vector2u(1u, 1u), rhi::EFragmentFormat::RGBA16_S_Float);
	skyViewProbeDef.type = rhi::ETextureType::Texture2D;
	const rg::RGTextureViewHandle skyProbe = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Sky Probe"), skyViewProbeDef);

	static const rdr::PipelineStateID pipeline = CompileRenderSkyProbePipeline();

	lib::MTHandle<RenderSkyProbeDS> ds = graphBuilder.CreateDescriptorSet<RenderSkyProbeDS>(RENDERER_RESOURCE_NAME("RenderSkyProbeDS"));
	ds->u_atmosphereParams = atmosphere.atmosphereParamsBuffer->CreateFullView();
	ds->u_skyViewLUT       = skyViewLUT;
	ds->u_rwProbe          = skyProbe;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Sky Probe"),
						  pipeline,
						  math::Vector2u(1u, 1u),
						  rg::BindDescriptorSets(std::move(ds)));
												

	return skyProbe;
}

} // render_sky_probe

namespace render_main_view
{

struct VolumetricCloudsParams
{
	const CloudscapeContext* cloudscape = nullptr;
};


BEGIN_SHADER_STRUCT(VolumetricCloudsMainViewConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, rcpResolution)
	SHADER_STRUCT_FIELD(Uint32,         frameIdx)
END_SHADER_STRUCT();


DS_BEGIN(RenderVolumetricCloudsMainViewDS, rg::RGDescriptorSetState<RenderVolumetricCloudsMainViewDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<VolumetricCloudsMainViewConstants>), u_passConstants)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                              u_blueNoise256)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                              u_depth)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                      u_skyProbe)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                               u_rwCloudsDepth)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                       u_rwClouds)
DS_END();


static rdr::PipelineStateID CompileRenderVolumetricCloudsMainViewPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Atmosphere/VolumetricClouds/RenderVolumetricCloudsMainView.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "RenderVolumetricCloudsMainViewCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("RenderVolumetricCloudsMainViewPipeline"), shader);
}

static void RenderVolumetricCloudsMainView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, ViewRenderingSpec& viewSpec, const VolumetricCloudsParams& params)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	ShadingViewContext& shadingViewContext = viewSpec.GetShadingViewContext();

	math::Vector2u cloudsResolution = viewSpec.GetRenderingRes();

	SPT_CHECK(!!params.cloudscape);

	const rg::RGTextureViewHandle skyProbe = render_sky_probe::RenderSkyProbe(graphBuilder, scene, viewSpec, params.cloudscape->atmosphere, shadingViewContext.skyViewLUT);

	VolumetricCloudsMainViewConstants shaderConstants;
	shaderConstants.resolution      = cloudsResolution;
	shaderConstants.rcpResolution   = cloudsResolution.cast<Real32>().cwiseInverse();
	shaderConstants.frameIdx        = renderView.GetRenderedFrameIdx();

	const rg::RGTextureViewHandle cloudsTexture      = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Clouds"), rg::TextureDef(cloudsResolution, rhi::EFragmentFormat::RGBA16_S_Float));
	const rg::RGTextureViewHandle cloudsDepthTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Clouds Depth"), rg::TextureDef(cloudsResolution, rhi::EFragmentFormat::R32_S_Float));

	static const rdr::PipelineStateID pipeline = CompileRenderVolumetricCloudsMainViewPipeline();

	lib::MTHandle<RenderVolumetricCloudsMainViewDS> ds = graphBuilder.CreateDescriptorSet<RenderVolumetricCloudsMainViewDS>(RENDERER_RESOURCE_NAME("RenderVolumetricCloudsMainViewDS"));
	ds->u_passConstants = shaderConstants;
	ds->u_blueNoise256  = gfx::global::Resources::Get().blueNoise256.GetView();
	ds->u_depth         = shadingViewContext.depth;
	ds->u_skyProbe      = skyProbe;
	ds->u_rwCloudsDepth = cloudsDepthTexture;
	ds->u_rwClouds      = cloudsTexture;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Volumetric Clouds Main View"),
						  pipeline,
						  math::Utils::DivideCeil(cloudsResolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds),
												 params.cloudscape->cloudscapeDS,
												 renderView.GetRenderViewDS()));

	shadingViewContext.volumetricClouds      = cloudsTexture;
	shadingViewContext.volumetricCloudsDepth = cloudsDepthTexture;

}

} // render_main_view

namespace sun_clouds_transmittance
{

struct CloudsTransmittanceMapParams
{
	const CloudscapeContext* cloudscape = nullptr;
};


BEGIN_SHADER_STRUCT(RenderCloudsTransmittanceMapConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
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
	const math::Vector2u resolution = math::Vector2u(2048u, 2048u);

	rg::RGTextureViewHandle cloudsTransmittanceTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Clouds Transmittance"), rg::TextureDef(resolution, rhi::EFragmentFormat::R16_UN_Float));

	static const rdr::PipelineStateID pipeline = CompileRenderCloudsTransmittanceMapPipeline();

	const AtmosphereContext& atmosphere = params.cloudscape->atmosphere;
	const DirectionalLightData& mainDirLight = *atmosphere.mainDirectionalLight;

	RenderCloudsTransmittanceMapConstants shaderConstants;
	shaderConstants.resolution    = resolution;
	shaderConstants.rcpResolution = resolution.cast<Real32>().cwiseInverse();
	shaderConstants.direction     = -mainDirLight.direction;

	BuildCloudsTransmittanceMapMatrices(mainDirLight.direction, 20000.f, 4000.f, OUT shaderConstants.viewProjectionMatrix);
	shaderConstants.invViewProjectionMatrix = shaderConstants.viewProjectionMatrix.inverse();

	lib::MTHandle<RenderCloudsTransmittanceMapDS> ds = graphBuilder.CreateDescriptorSet<RenderCloudsTransmittanceMapDS>(RENDERER_RESOURCE_NAME("RenderCloudsTransmittanceMapDS"));
	ds->u_rwTransmittanceMap = cloudsTransmittanceTexture;
	ds->u_constants          = shaderConstants;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Clouds Sun Transmittance Map"),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(ds, params.cloudscape->cloudscapeDS));

	CloudsTransmittanceMap cloudsTransmittanceMap
	{
		.cloudsTransmittanceTexture = cloudsTransmittanceTexture,
		.viewProjectionMatrix = shaderConstants.viewProjectionMatrix
	};

	return cloudsTransmittanceMap;
}

} // sun_clouds_transmittance

VolumetricCloudsRenderer::VolumetricCloudsRenderer()
{
	InitTextures();

	const Real32 volumetricCloudsMinHeight = 1500.f;
	const Real32 volumetricCloudsMaxHeight = 4000.f;
	const Real32 volumetricCloudsRange     = 20000.f;

	const Real32 radius = (volumetricCloudsRange * volumetricCloudsRange + volumetricCloudsMinHeight * volumetricCloudsMinHeight) / (2.f * volumetricCloudsMinHeight);

	m_cloudscapeConstants.cloudsAtmosphereCenterZ     = volumetricCloudsMinHeight - radius;
	m_cloudscapeConstants.cloudsAtmosphereInnerRadius = radius;
	m_cloudscapeConstants.cloudsAtmosphereOuterRadius = radius + (volumetricCloudsMaxHeight - volumetricCloudsMinHeight);
	m_cloudscapeConstants.cloudscapeRange             = volumetricCloudsRange;
	m_cloudscapeConstants.cloudscapeInnerHeight       = volumetricCloudsMinHeight;
	m_cloudscapeConstants.cloudscapeOuterHeight       = volumetricCloudsMaxHeight;

	SPT_LOG_INFO(VolumetricCloudsRenderer, "Computed clouds atmosphere radius: {}", radius);
}

void VolumetricCloudsRenderer::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs)
{
	m_volumetricCloudsEnabled = params::enableVolumetricClouds;

	if (m_volumetricCloudsEnabled)
	{
		const CloudscapeContext cloudscapeContext = CreateFrameCloudscapeContext(graphBuilder, renderScene, viewSpecs);

		sun_clouds_transmittance::CloudsTransmittanceMapParams cloudsTransmittanceParams;
		cloudsTransmittanceParams.cloudscape = &cloudscapeContext;
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
}

void VolumetricCloudsRenderer::RenderVolumetricClouds(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context, const CloudscapeContext cloudscapeContext)
{
	SPT_PROFILER_FUNCTION();

	render_main_view::VolumetricCloudsParams clodusMainViewParams;
	clodusMainViewParams.cloudscape = &cloudscapeContext;
	render_main_view::RenderVolumetricCloudsMainView(graphBuilder, scene, viewSpec, clodusMainViewParams);
}

CloudscapeContext VolumetricCloudsRenderer::CreateFrameCloudscapeContext(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs)
{
	SPT_CHECK(!viewSpecs.empty());

	const ViewRenderingSpec* mainView = viewSpecs[0];

	const AtmosphereSceneSubsystem& atmosphereSubsystem = renderScene.GetSceneSubsystemChecked<AtmosphereSceneSubsystem>();

	const AtmosphereContext& atmosphere = atmosphereSubsystem.GetAtmosphereContext();

	const RenderView& renderView = mainView->GetRenderView();
	const math::Vector3f viewLocation = renderView.GetLocation();

	const math::Vector3f cloudsAtmosphereCenter = math::Vector3f(viewLocation.x(), viewLocation.y(), m_cloudscapeConstants.cloudsAtmosphereCenterZ);

	m_cloudscapeConstants.baseShapeNoiseScale    = 1.f / params::baseShapeNoiseMeters;
	m_cloudscapeConstants.detailShapeNoiseScale  = 1.f / params::detailShapeNoiseMeters;
	m_cloudscapeConstants.weatherMapScale        = 1.f / 40000.f;
	m_cloudscapeConstants.cloudsAtmosphereCenter = cloudsAtmosphereCenter;
	m_cloudscapeConstants.globalDensity          = params::globalDensity;
	m_cloudscapeConstants.globalCoverage         = params::globalCoverage;
	m_cloudscapeConstants.time                   = renderScene.GetCurrentFrameRef().GetTime();

	lib::MTHandle<CloudscapeDS> ds = graphBuilder.CreateDescriptorSet<CloudscapeDS>(RENDERER_RESOURCE_NAME("CloudscapeDS"));
	ds->u_cloudscapeConstants = m_cloudscapeConstants;
	ds->u_atmosphereConstants = atmosphere.atmosphereParamsBuffer->CreateFullView();
	ds->u_directionalLights   = atmosphere.directionalLightsBuffer->CreateFullView();
	ds->u_transmittanceLUT    = atmosphere.transmittanceLUT;
	ds->u_baseShapeNoise      = graphBuilder.AcquireExternalTextureView(m_baseShapeNoiseTexture);
	ds->u_detailShapeNoise    = graphBuilder.AcquireExternalTextureView(m_detailShapeNoiseTexture);
	ds->u_curlNoise           = graphBuilder.AcquireExternalTextureView(m_curlNoise);
	ds->u_weatherMap          = graphBuilder.AcquireExternalTextureView(m_weatherMap);

	return CloudscapeContext{ atmosphere, m_cloudscapeConstants, ds };

}

void VolumetricCloudsRenderer::InitTextures()
{
	LoadOrCreateBaseShapeNoiseTexture();
	LoadOrCreateDetailShapeNoiseTexture();
	LoadWeatherMapTexture();
	LoadCurlNoiseTexture();
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

		texture = gfx::TextureLoader::LoadTexture(texturePath);
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
	const lib::String texturePath = engn::Paths::GetContentPath() + "/RenderingPipeline/Textures/Clouds/WeatherMap.png";

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

} // spt::rsc::clouds
