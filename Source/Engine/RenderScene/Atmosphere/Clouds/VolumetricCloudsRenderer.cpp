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

RendererFloatParameter baseShapeNoiseMeters("Base Shape Noise Meters", { "Clouds" }, 800.f, 0.f, 60000.f);
RendererFloatParameter cirrusCloudsMeters("Cirrus Clouds Meters", { "Clouds" }, 40000.f, 0.f, 60000.f);

RendererFloatParameter detailShapeNoiseStrength0("Detail Shape Noise Strength (Octave 0)", { "Clouds" }, 0.45f, 0.f, 2.f);
RendererFloatParameter detailShapeNoiseMeters0("Detail Shape Noise Meters (Octave 0)", { "Clouds" }, 255.f, 0.f, 20000.f);
RendererFloatParameter detailShapeNoiseStrength1("Detail Shape Noise Strength (Octave 1)", { "Clouds" }, 0.0f, 0.f, 2.f);
RendererFloatParameter detailShapeNoiseMeters1("Detail Shape Noise Meters (Octave 1)", { "Clouds" }, 255.f, 0.f, 20000.f);

RendererFloatParameter curlNoiseOffset("Curl Noise Offset", { "Clouds" }, 5.f, 0.f, 500.f);
RendererFloatParameter curlNoiseMeters("Curl Noise Meters", { "Clouds" }, 2222.f, 0.f, 40000.f);

RendererFloatParameter globalDensity("Global Density", { "Clouds" }, 1.f, 0.f, 5.f);
RendererFloatParameter globalCoverageOffset("Global Coverage Offset", { "Clouds" }, 0.4f, -1.f, 1.f);
RendererFloatParameter cloudsHeightOffset("Clouds Height Offset", { "Clouds" }, 0.0f, -1.f, 1.f);

RendererBoolParameter enableVolumetricClouds("Enable Volumetric Clouds", { "Clouds" }, true);

} // params


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
	ds->u_skyProbe      = shadingViewContext.skyProbe;
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
	const DirectionalLightGPUData& mainDirLight = *atmosphere.mainDirectionalLight;

	const CloudscapeConstants& cloudscapeConstants = params.cloudscape->cloudscapeConstants;

	RenderCloudsTransmittanceMapConstants shaderConstants;
	shaderConstants.resolution    = resolution;
	shaderConstants.rcpResolution = resolution.cast<Real32>().cwiseInverse();
	shaderConstants.direction     = -mainDirLight.direction;

	math::Vector3f center = params.cloudscape->cloudscapeConstants.cloudsAtmosphereCenter;
	center.z() = 0.f;

	BuildCloudsTransmittanceMapMatrices(center, mainDirLight.direction, cloudscapeConstants.cloudscapeRange, cloudscapeConstants.cloudscapeOuterHeight, OUT shaderConstants.viewProjectionMatrix);
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


static rdr::PipelineStateID CompileTraceCloudscapeProbesPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Atmosphere/VolumetricClouds/TraceCloudscapeProbes.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "TraceCloudscapeProbesCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("TraceCloudscapeProbesPipeline"), shader);
}


rg::RGTextureViewHandle TraceCloudscapeProbes(rg::RenderGraphBuilder& graphBuilder, const ProbesUpdateParams& params)
{
	SPT_PROFILER_FUNCTION();

	math::Vector2u traceResultRes = math::Vector2u(params.tracesPerProbe, maxProbesToUpdate);

	const rg::RGTextureViewHandle traceResult = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Cloudscape Probes Trace"), rg::TextureDef(traceResultRes, rhi::EFragmentFormat::RGBA16_S_Float));

	static const rdr::PipelineStateID pipeline = CompileTraceCloudscapeProbesPipeline();

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
						  math::Utils::DivideCeil(traceResultRes, math::Vector2u(64u, 1u)),
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
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                     u_rwCompressedSimpleProbes)
DS_END();


static rdr::PipelineStateID CompileCompressCloudscapeProbesPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Atmosphere/VolumetricClouds/CompressCloudscapeProbes.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CompressCloudscapeProbesCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("CompressCloudscapeProbesPipeline"), shader);
}


void CompressCloudscapeProbes(rg::RenderGraphBuilder& graphBuilder, const ProbesUpdateParams& params, rg::RGTextureViewHandle traceResult)
{
	SPT_PROFILER_FUNCTION();

	static const rdr::PipelineStateID pipeline = CompileCompressCloudscapeProbesPipeline();

	UpdateCloudscapeProbesConstants shaderConstants;
	shaderConstants.raysPerProbe           = params.tracesPerProbe;
	shaderConstants.compressedProbeDataRes = params.cloudscape->cloudscapeConstants.pixelsPerProbe;
	shaderConstants.probesToUpdate         = params.probeCoords;

	lib::MTHandle<CompressCloudscapeProbesDS> ds = graphBuilder.CreateDescriptorSet<CompressCloudscapeProbesDS>(RENDERER_RESOURCE_NAME("CompressCloudscapeProbesDS"));
	ds->u_rwCompressedProbes       = params.cloduscapeProbesTexture;
	ds->u_rwCompressedSimpleProbes = params.cloduscapeSimpleProbesTexture;
	ds->u_traceResult              = traceResult;
	ds->u_constants                = shaderConstants;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Compress Cloudscape Probes"),
						  pipeline,
						  math::Vector2u(1u, maxProbesToUpdate),
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
	SHADER_STRUCT_FIELD(Uint32, frameIdx)
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
};


void UpdateCloudscapeHighResProbe(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, const UpdateCloudscapeHighResProbeParams& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.cloudscapeProbe->GetResolution2D();

	static const rdr::PipelineStateID pipeline = CompileUpdateCloudscapeHighResProbePipeline();

	const Uint32 frameIdx = viewSpec.GetRenderView().GetRenderedFrameIdx();

	UpdateCloudscapeHighResProbeConstants shaderConstants;
	shaderConstants.frameIdx = frameIdx;

	lib::MTHandle<UpdateCloudscapeHighResProbeDS> ds = graphBuilder.CreateDescriptorSet<UpdateCloudscapeHighResProbeDS>(RENDERER_RESOURCE_NAME("UpdateCloudscapeHighResProbeDS"));
	ds->u_constants    = shaderConstants;
	ds->u_skyViewProbe = params.skyProbe;
	ds->u_blueNoise256 = gfx::global::Resources::Get().blueNoise256.GetView();
	ds->u_rwProbe      = params.cloudscapeProbe;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Update High Res Probe"),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds),
												 viewSpec.GetRenderView().GetRenderViewDS(),
												 params.cloudscape->cloudscapeDS));
}

} // high_res

} // cloudscape_probe

VolumetricCloudsRenderer::VolumetricCloudsRenderer()
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
	m_volumetricCloudsEnabled = params::enableVolumetricClouds;

	if (m_volumetricCloudsEnabled)
	{
		const CloudscapeContext cloudscapeContext = CreateFrameCloudscapeContext(graphBuilder, renderScene, viewSpecs, settings);

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

	if (m_enqueuedCloudscapeProbesGlobalUpdate || cloudscapeContext.resetAccumulation)
	{
		UpdateAllCloudscapeProbes(graphBuilder, renderScene, viewSpec, cloudscapeContext);
		m_enqueuedCloudscapeProbesGlobalUpdate = false;
	}
	else
	{
		UpdateCloudscapeProbesPerFrame(graphBuilder, renderScene, viewSpec, cloudscapeContext);
	}

	const rg::RGTextureViewHandle cloudscapeProbes       = graphBuilder.TryAcquireExternalTextureView(m_cloudscapeProbes);
	const rg::RGTextureViewHandle cloudscapeSimpleProbes = graphBuilder.TryAcquireExternalTextureView(m_cloudscapeSimpleProbes);
	const rg::RGTextureViewHandle cloudscapeHighResProbe = graphBuilder.TryAcquireExternalTextureView(m_cloudscapeHighResProbe);

	cloudscape_probe::high_res::UpdateCloudscapeHighResProbeParams highResProbeParams;
	highResProbeParams.cloudscape      = &cloudscapeContext;
	highResProbeParams.skyProbe        = viewSpec.GetShadingViewContext().skyProbe;
	highResProbeParams.cloudscapeProbe = cloudscapeHighResProbe;

	cloudscape_probe::high_res::UpdateCloudscapeHighResProbe(graphBuilder, viewSpec, highResProbeParams);

	ShadingViewContext& shadingViewContext = viewSpec.GetShadingViewContext();

	lib::MTHandle<CloudscapeProbesDS> cloudscapeFullDS = graphBuilder.CreateDescriptorSet<CloudscapeProbesDS>(RENDERER_RESOURCE_NAME("CloudscapeProbesDS"));
	cloudscapeFullDS->u_cloudscapeConstants    = cloudscapeContext.cloudscapeConstants;
	cloudscapeFullDS->u_cloudscapeProbes       = cloudscapeProbes;
	cloudscapeFullDS->u_cloudscapeSimpleProbes = cloudscapeSimpleProbes;
	cloudscapeFullDS->u_cloudscapeHighResProbe = cloudscapeHighResProbe;

	shadingViewContext.cloudscapeProbesDS = std::move(cloudscapeFullDS);
}

void VolumetricCloudsRenderer::RenderVolumetricClouds(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context, const CloudscapeContext cloudscapeContext)
{
	SPT_PROFILER_FUNCTION();

	render_main_view::VolumetricCloudsParams clodusMainViewParams;
	clodusMainViewParams.cloudscape = &cloudscapeContext;
	render_main_view::RenderVolumetricCloudsMainView(graphBuilder, scene, viewSpec, clodusMainViewParams);
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

	m_cloudscapeConstants.baseShapeNoiseScale       = 1.f / params::baseShapeNoiseMeters;
	m_cloudscapeConstants.detailShapeNoiseStrength0 = params::detailShapeNoiseStrength0;
	m_cloudscapeConstants.detailShapeNoiseScale0    = 1.f / params::detailShapeNoiseMeters0;
	m_cloudscapeConstants.detailShapeNoiseStrength1 = params::detailShapeNoiseStrength1;
	m_cloudscapeConstants.detailShapeNoiseScale1    = 1.f / params::detailShapeNoiseMeters1;
	m_cloudscapeConstants.curlNoiseScale            = 1.f / params::curlNoiseMeters;
	m_cloudscapeConstants.curlMaxoffset             = params::curlNoiseOffset;
	m_cloudscapeConstants.weatherMapScale           = 1.f / 16000.f;
	m_cloudscapeConstants.cloudsAtmosphereCenter    = cloudsAtmosphereCenter;
	m_cloudscapeConstants.globalDensity             = params::globalDensity;
	m_cloudscapeConstants.globalCoverageOffset      = params::globalCoverageOffset;
	m_cloudscapeConstants.globalCloudsHeightOffset = params::cloudsHeightOffset;
	m_cloudscapeConstants.time                      = renderScene.GetCurrentFrameRef().GetTime();
	m_cloudscapeConstants.mainDirectionalLight      = *atmosphere.mainDirectionalLight;

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

	return CloudscapeContext{ atmosphere, m_cloudscapeConstants, ds, resetAccumulation };
}

void VolumetricCloudsRenderer::UpdateAllCloudscapeProbes(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const CloudscapeContext& cloudscapeContext)
{
	SPT_PROFILER_FUNCTION();

	const Uint32 probesNum = cloudscapeContext.cloudscapeConstants.probesNum.x() * cloudscapeContext.cloudscapeConstants.probesNum.y();
	const Uint32 batchesNum = math::Utils::DivideCeil(probesNum, cloudscape_probe::maxProbesToUpdate);

	for (Uint32 batchIdx = 0u; batchIdx < batchesNum; ++batchIdx)
	{
		lib::StaticArray<math::Vector2u, cloudscape_probe::maxProbesToUpdate> probeCoords;
		for (Uint32 i = 0u; i < cloudscape_probe::maxProbesToUpdate; ++i)
		{
			const Uint32 probeIdx = batchIdx * cloudscape_probe::maxProbesToUpdate + i;
			probeCoords[i] = math::Vector2u(probeIdx % cloudscapeContext.cloudscapeConstants.probesNum.x(), probeIdx / cloudscapeContext.cloudscapeConstants.probesNum.x());
		}

		UpdateCloudscapeProbes(graphBuilder, renderScene, viewSpec, cloudscapeContext, probeCoords);
	}
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

	UpdateCloudscapeProbes(graphBuilder, renderScene, viewSpec, cloudscapeContext, probeCoords);
}

void VolumetricCloudsRenderer::UpdateCloudscapeProbes(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const CloudscapeContext& cloudscapeContext, lib::Span<const math::Vector2u> probeCoords)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(probeCoords.size() <= cloudscape_probe::maxProbesToUpdate);

	const rg::RGTextureViewHandle cloudscapeProbes       = graphBuilder.AcquireExternalTextureView(m_cloudscapeProbes);
	const rg::RGTextureViewHandle cloudscapeSimpleProbes = graphBuilder.AcquireExternalTextureView(m_cloudscapeSimpleProbes);

	ShadingViewContext& shadingViewContext = viewSpec.GetShadingViewContext();

	cloudscape_probe::ProbesUpdateParams updateParams;
	updateParams.cloudscape                    = &cloudscapeContext;
	updateParams.cloduscapeProbesTexture       = cloudscapeProbes;
	updateParams.cloduscapeSimpleProbesTexture = cloudscapeSimpleProbes;
	updateParams.tracesPerProbe                = 1024u;
	updateParams.skyProbe                      = shadingViewContext.skyProbe;

	for (Uint32 i = 0u; i < probeCoords.size(); ++i)
	{
		updateParams.probeCoords[i] = math::Vector4u(probeCoords[i].x(), probeCoords[i].y(), 0u, 0u);
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

	rhi::TextureDefinition cloudscapeSimpleProbesDef;
	cloudscapeSimpleProbesDef.resolution = probesRes;
	cloudscapeSimpleProbesDef.format     = rhi::EFragmentFormat::RGBA16_S_Float;
	cloudscapeSimpleProbesDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferDest);
	m_cloudscapeSimpleProbes = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("Cloudscape Simple Probes"), cloudscapeSimpleProbesDef, rhi::EMemoryUsage::GPUOnly);

	rhi::TextureDefinition cloudscapeHighResProbeDef;
	cloudscapeHighResProbeDef.resolution = m_cloudscapeConstants.highResProbeRes;
	cloudscapeHighResProbeDef.format     = rhi::EFragmentFormat::RGBA16_S_Float;
	cloudscapeHighResProbeDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferDest);
	m_cloudscapeHighResProbe = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("Cloudscape High Res Probe"), cloudscapeHighResProbeDef, rhi::EMemoryUsage::GPUOnly);
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
