#include "DDGIRenderSystem.h"
#include "SceneRenderSystems/Atmosphere/AtmosphereRenderSystem.h"
#include "RenderScene.h"
#include "RenderGraphBuilder.h"
#include "ResourcesManager.h"
#include "Utils/BufferUtils.h"
#include "Common/ShaderCompilationInput.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "SceneRenderSystems/Atmosphere/AtmosphereTypes.h"
#include "MaterialsSubsystem.h"
#include "SceneRenderSystems/Lights/LightsRenderSystem.h"
#include "DDGIVolume.h"
#include "Pipelines/PSOsLibraryTypes.h"
#include "ConfigUtils.h"


namespace spt::rsc::ddgi
{

SPT_REGISTER_SCENE_RENDER_SYSTEM(DDGIRenderSystem);

namespace renderer_params
{
RendererBoolParameter ddgiEnabled("Enable DDGI", {"DDGI"}, true);
} // renderer_params

//////////////////////////////////////////////////////////////////////////////////////////////////
// Descriptor Sets ===============================================================================

BEGIN_SHADER_STRUCT(DDGIBlendParams)
	SHADER_STRUCT_FIELD(math::Vector2f, traceRaysResultTexturePixelSize)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(DDGIDebugRay)
	SHADER_STRUCT_FIELD(math::Vector3f, traceOrigin)
	SHADER_STRUCT_FIELD(Bool,			isValidHit)
	SHADER_STRUCT_FIELD(math::Vector3f, traceEnd)
	SHADER_STRUCT_FIELD(math::Vector3f, luminance)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(DDGITraceRaysConsts)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>, skyViewLUT)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<AtmosphereParams>,     atmosphereParams)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>, transmittanceLUT)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, traceRaysResultTexture)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<DDGIRelitGPUParams>,   relitParams)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<DDGIVolumeGPUParams>,  volumeParams)
END_SHADER_STRUCT();


template<typename T>
using VolumeUAVTexturesArray = lib::StaticArray<gfx::UAVTexture2D<T>, constants::maxTexturesPerVolume>;

template<typename T>
using VolumeSRVTexturesArray = lib::StaticArray<gfx::SRVTexture2D<T>, constants::maxTexturesPerVolume>;


BEGIN_SHADER_STRUCT(DDGIBlendProbesDataConsts)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>,      traceRaysResultTexture)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<DDGIRelitGPUParams>,        relitParams)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<DDGIVolumeGPUParams>,       volumeParams)
	SHADER_STRUCT_FIELD(VolumeUAVTexturesArray<math::Vector4f>, volumeTextures)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(DDGIInvalidateProbesConsts)
	SHADER_STRUCT_FIELD(math::Vector3f,                          prevAABBMin)
	SHADER_STRUCT_FIELD(math::Vector3f,                          prevAABBMax)
	SHADER_STRUCT_FIELD(Bool,                                    forceInvalidateAll)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<DDGIVolumeGPUParams>,        volumeParams)
	SHADER_STRUCT_FIELD(VolumeUAVTexturesArray<math::Vector4f>,  volumeHitDistanceTextures)
	SHADER_STRUCT_FIELD(VolumeUAVTexturesArray<math::Vector4f>,  volumeIlluminanceTextures)
	SHADER_STRUCT_FIELD(gfx::UAVTexture3D<math::Vector4f>,       volumeProbesAverageLuminanceTexture)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(DDGIUpdateProbesAverageLuminanceConsts)
	SHADER_STRUCT_FIELD(VolumeSRVTexturesArray<math::Vector3f>, volumeIlluminanceTextures)
	SHADER_STRUCT_FIELD(gfx::UAVTexture3D<math::Vector3f>,      probesAverageLuminanceTexture)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<DDGIRelitGPUParams>,        relitParams)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<DDGIVolumeGPUParams>,       volumeParams)
END_SHADER_STRUCT();

//////////////////////////////////////////////////////////////////////////////////////////////////
// Pipelines =====================================================================================

namespace pipelines
{

RT_PSO(DDGITraceRaysPSO)
{
	RAY_GEN_SHADER("Sculptor/DDGI/DDGITraceRays.hlsl", DDGIProbeRaysRTG);
	MISS_SHADERS(SHADER_ENTRY("Sculptor/DDGI/DDGITraceRays.hlsl",  GenericRTM));

	HIT_GROUP
	{
		CLOSEST_HIT_SHADER("Sculptor/DDGI/DDGITraceRays.hlsl", GenericCHS);
		ANY_HIT_SHADER("Sculptor/DDGI/DDGITraceRays.hlsl", GenericAH);

		HIT_PERMUTATION_DOMAIN(mat::RTHitGroupPermutation);
	};

	PRESET(pso);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		const rhi::RayTracingPipelineDefinition psoDefinition{ .maxRayRecursionDepth = 1u };
		pso = CompilePSO(compiler, psoDefinition, mat::MaterialsSubsystem::Get().GetRTHitGroups<HitGroup>());
	}
};

static rdr::PipelineStateID CreateDDGIBlendProbesIlluminancePipeline(math::Vector2u groupSize, Uint32 raysNumPerProbe)
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("DDGI_BLEND_TYPE", "1")); // Blend Illuminance
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("GROUP_SIZE_X", std::to_string(groupSize.x())));
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("GROUP_SIZE_Y", std::to_string(groupSize.y())));
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("RAYS_NUM_PER_PROBE", std::to_string(raysNumPerProbe)));

	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/DDGI/DDGIBlendProbesData.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "DDGIBlendProbesDataCS"), compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("DDGI Blend Probes Illuminance Pipeline"), shader);
}

static rdr::PipelineStateID CreateDDGIBlendProbesHitDistancePipeline(math::Vector2u groupSize, Uint32 raysNumPerProbe)
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("DDGI_BLEND_TYPE", "2")); // Blend hit distance
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("GROUP_SIZE_X", std::to_string(groupSize.x())));
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("GROUP_SIZE_Y", std::to_string(groupSize.y())));
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("RAYS_NUM_PER_PROBE", std::to_string(raysNumPerProbe)));

	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/DDGI/DDGIBlendProbesData.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "DDGIBlendProbesDataCS"), compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("DDGI Blend Probes Hit Distance Pipeline"), shader);
}

static rdr::PipelineStateID CreateDDGIUpdateProbesAverageLuminancePipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/DDGI/DDGIUpdateProbesAverageLuminance.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "DDGIUpdateProbesAverageLuminanceCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("DDGI Update Probes Average Luminance Pipeline"), shader);
}

static rdr::PipelineStateID CreateDDGIInvalidateProbesPipeline(math::Vector2u groupSize)
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("GROUP_SIZE_X", std::to_string(groupSize.x())));
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("GROUP_SIZE_Y", std::to_string(groupSize.y())));

	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/DDGI/DDGIInvalidateProbes.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "DDGIInvalidateProbesCS"), compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("DDGI Invalidate Probes Pipeline"), shader);
}

} // pipelines


//////////////////////////////////////////////////////////////////////////////////////////////////
// DDGIUpdateParameters ==========================================================================

DDGIVolumeRelitParameters::DDGIVolumeRelitParameters(rg::RenderGraphBuilder& graphBuilder, const DDGIRelitGPUParams& relitParams, const DDGIScene& ddgiScene, const DDGIVolume& inVolume)
	: volume(inVolume)
	, probesAverageLuminanceTextureView(graphBuilder.AcquireExternalTextureView(volume.GetProbesAverageLuminanceTexture()))
	, relitParams(graphBuilder.CreateGPUData(relitParams))
	, volumeParams(graphBuilder.CreateGPUData(volume.GetVolumeGPUParams()))
	, probesNumToUpdate(relitParams.probesNumToUpdate)
	, raysNumPerProbe(relitParams.raysNumPerProbe)
	, probeIlluminanceDataWithBorderRes(volume.GetVolumeGPUParams().probeIlluminanceDataWithBorderRes)
	, probeHitDistanceDataWithBorderRes(volume.GetVolumeGPUParams().probeHitDistanceDataWithBorderRes)
	, ddgiGPUScene(ddgiScene.GetDDGIGPUScene())
{
	const Uint32 probeDataTexturesNum = volume.GetProbeDataTexturesNum();

	probesIlluminanceTextureViews.reserve(probeDataTexturesNum);
	probesHitDistanceTextureViews.reserve(probeDataTexturesNum);
	for (Uint32 textureIdx = 0u; textureIdx < probeDataTexturesNum; ++textureIdx)
	{
		probesIlluminanceTextureViews.emplace_back(graphBuilder.AcquireExternalTextureView(volume.GetProbesIlluminanceTexture(textureIdx)));
		probesHitDistanceTextureViews.emplace_back(graphBuilder.AcquireExternalTextureView(volume.GetProbesHitDistanceTexture(textureIdx)));
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// DDGI Utilities ================================================================================

struct DDGIDebugRaysViewData
{
	struct DebugRaysBuffer
	{
		explicit DebugRaysBuffer(rg::RGBufferViewHandle inDebugRays, Uint32 inRaysNum)
			: debugRays(inDebugRays)
			, raysNum(inRaysNum)
		{ }

		rg::RGBufferViewHandle debugRays;

		Uint32 raysNum;
	};

	DDGIDebugRaysViewData() = default;

	lib::DynamicArray<DebugRaysBuffer> debugRaysBuffers;
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// DDGIRenderSystem ==============================================================================

DDGIRenderSystem::DDGIRenderSystem(lib::MemoryArena& arena, RenderScene& owningScene)
	: Super(arena, owningScene)
	, m_ddgiScene(owningScene)
{
	engn::ConfigUtils::LoadConfigData(m_config, "DDGIConfig.json");

	m_ddgiScene.Initialize(m_config);

	m_supportedStages = lib::Flags(ERenderStage::GlobalIllumination, ERenderStage::DeferredShading);
}

void DDGIRenderSystem::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const lib::DynamicPushArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings)
{
	SPT_PROFILER_FUNCTION();

	Super::RenderPerFrame(graphBuilder, rendererInterface, renderScene, viewSpecs, settings);

	m_ddgiScene.Update(graphBuilder, (*viewSpecs.begin())->GetRenderView());

	for (ViewRenderingSpec* view : viewSpecs)
	{
		SPT_CHECK(!!view);
		RenderPerView(graphBuilder, rendererInterface, renderScene, *view);
	}
}

Bool DDGIRenderSystem::IsDDGIEnabled() const
{
	return renderer_params::ddgiEnabled;
}

void DDGIRenderSystem::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	if (viewSpec.SupportsStage(ERenderStage::GlobalIllumination))
	{
		viewSpec.GetRenderViewEntry(ERenderViewEntry::RenderGI).AddRawMember(this, &DDGIRenderSystem::RenderGlobalIllumination);
	}
}

void DDGIRenderSystem::RelitScene(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RelitSettings& settings)
{
	SPT_PROFILER_FUNCTION();

	InvalidateVolumes(graphBuilder, rendererInterface, renderScene, viewSpec, settings);

	const SizeType relitBudget = settings.reset ? maxValue<SizeType> : m_config.relitVolumesBudget;

	DDGIZonesCollector zonesCollector(relitBudget);

	m_ddgiScene.CollectZonesToRelit(zonesCollector);

	const lib::DynamicArray<DDGIRelitZone*>& zones = zonesCollector.GetZonesToRelit();

	for (DDGIRelitZone* zone : zones)
	{
		SPT_CHECK(!!zone);

		RelitZone(graphBuilder, rendererInterface, renderScene, viewSpec, *zone);
	}

	m_ddgiScene.PostRelit();
}

void DDGIRenderSystem::InvalidateVolumes(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RelitSettings& settings) const
{
	SPT_PROFILER_FUNCTION();

	const lib::DynamicArray<DDGIVolume*>& volumes = m_ddgiScene.GetVolumes();

	for (DDGIVolume* volume : volumes)
	{
		SPT_CHECK(!!volume);

		if (volume->RequiresInvalidation() || settings.reset)
		{
			InvalidateOutOfBoundsProbes(graphBuilder, rendererInterface, renderScene, viewSpec, *volume, settings);
		}
	}
}

void DDGIRenderSystem::RelitZone(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, DDGIRelitZone& zone) const
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "Relit Zone");

	const DDGIVolumeRelitParameters relitParams(graphBuilder, CreateRelitParams(zone, m_config), m_ddgiScene, zone.GetVolume());

	UpdateProbes(graphBuilder, rendererInterface, renderScene, viewSpec, relitParams);

	zone.PostRelit();
}

void DDGIRenderSystem::UpdateProbes(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const DDGIVolumeRelitParameters& relitParams) const
{
	SPT_PROFILER_FUNCTION();

	const rg::RGTextureViewHandle probesTraceResultTexture = TraceRays(graphBuilder, rendererInterface, renderScene, viewSpec, relitParams);

	DDGIBlendProbesDataConsts updateProbesConsts;
	updateProbesConsts.traceRaysResultTexture = probesTraceResultTexture;
	updateProbesConsts.relitParams            = relitParams.relitParams;
	updateProbesConsts.volumeParams           = relitParams.volumeParams;

	for (Uint32 textureIdx = 0u; textureIdx < relitParams.probesIlluminanceTextureViews.size(); ++textureIdx)
	{
		updateProbesConsts.volumeTextures[textureIdx] = relitParams.probesIlluminanceTextureViews[textureIdx];
	}

	const rdr::PipelineStateID updateProbesIlluminancePipelineID = pipelines::CreateDDGIBlendProbesIlluminancePipeline(relitParams.probeIlluminanceDataWithBorderRes, relitParams.raysNumPerProbe);
	
	graphBuilder.Dispatch(RG_DEBUG_NAME("DDGI Update Probes Illuminance"),
						  updateProbesIlluminancePipelineID,
						  math::Vector3u(relitParams.probesNumToUpdate, 1u, 1u),
						  rg::ShaderParams(updateProbesConsts));

	for (Uint32 textureIdx = 0u; textureIdx < relitParams.probesHitDistanceTextureViews.size(); ++textureIdx)
	{
		updateProbesConsts.volumeTextures[textureIdx] = relitParams.probesHitDistanceTextureViews[textureIdx];
	}
	
	const rdr::PipelineStateID updateProbesDistancesPipelineID = pipelines::CreateDDGIBlendProbesHitDistancePipeline(relitParams.probeHitDistanceDataWithBorderRes, relitParams.raysNumPerProbe);;

	graphBuilder.Dispatch(RG_DEBUG_NAME("DDGI Update Probes Hit Distance"),
						  updateProbesDistancesPipelineID,
						  math::Vector3u(relitParams.probesNumToUpdate, 1u, 1u),
						  rg::ShaderParams(updateProbesConsts));

	DDGIUpdateProbesAverageLuminanceConsts updateProbesAverageLuminanceConsts;
	updateProbesAverageLuminanceConsts.probesAverageLuminanceTexture = relitParams.probesAverageLuminanceTextureView;
	updateProbesAverageLuminanceConsts.relitParams                   = relitParams.relitParams;
	updateProbesAverageLuminanceConsts.volumeParams                  = relitParams.volumeParams;

	for (Uint32 textureIdx = 0u; textureIdx < relitParams.probesIlluminanceTextureViews.size(); ++textureIdx)
	{
		updateProbesAverageLuminanceConsts.volumeIlluminanceTextures[textureIdx] = relitParams.probesIlluminanceTextureViews[textureIdx];
	}

	const rdr::PipelineStateID updateProbesAverageLuminancePipelineID = pipelines::CreateDDGIUpdateProbesAverageLuminancePipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("DDGI Update Probes Average Luminance"),
						  updateProbesAverageLuminancePipelineID,
						  math::Vector3u(relitParams.probesNumToUpdate, 1u, 1u),
						  rg::ShaderParams(updateProbesAverageLuminanceConsts));
}

void DDGIRenderSystem::InvalidateOutOfBoundsProbes(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, DDGIVolume& volume, const RelitSettings& settings) const
{
	SPT_PROFILER_FUNCTION();

	const math::Vector3u volumeRes = volume.GetProbesVolumeResolution();

	const DDGIVolumeGPUParams& volumeParams = volume.GetVolumeGPUParams();

	const math::AlignedBox3f prevAABB = volume.GetPrevAABB();

	DDGIInvalidateProbesConsts invalidateProbesConsts;
	invalidateProbesConsts.prevAABBMin                         = prevAABB.min();
	invalidateProbesConsts.prevAABBMax                         = prevAABB.max();
	invalidateProbesConsts.forceInvalidateAll                  = settings.reset;
	invalidateProbesConsts.volumeParams                        = graphBuilder.CreateGPUData(volumeParams);
	invalidateProbesConsts.volumeProbesAverageLuminanceTexture = volume.GetProbesAverageLuminanceTexture();

	const Uint32 probeDataTexturesNum = volume.GetProbeDataTexturesNum();

	for (Uint32 textureIdx = 0u; textureIdx < probeDataTexturesNum; ++textureIdx)
	{
		const rg::RGTextureViewHandle textureView = graphBuilder.AcquireExternalTextureView(volume.GetProbesHitDistanceTexture(textureIdx));
		invalidateProbesConsts.volumeIlluminanceTextures[textureIdx] = textureView;
	}

	for (Uint32 textureIdx = 0u; textureIdx < probeDataTexturesNum; ++textureIdx)
	{
		const rg::RGTextureViewHandle textureView = graphBuilder.AcquireExternalTextureView(volume.GetProbesHitDistanceTexture(textureIdx));
		invalidateProbesConsts.volumeHitDistanceTextures[textureIdx] = textureView;
	}

	static const rdr::PipelineStateID invalidateProbesPipelineID = pipelines::CreateDDGIInvalidateProbesPipeline(volumeParams.probeHitDistanceDataWithBorderRes);

	graphBuilder.Dispatch(RG_DEBUG_NAME("DDGI Invalidate Probes"),
						  invalidateProbesPipelineID,
						  volumeRes,
						  rg::ShaderParams(invalidateProbesConsts));

	volume.PostInvalidation();
}

rg::RGTextureViewHandle DDGIRenderSystem::TraceRays(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const DDGIVolumeRelitParameters& relitParams) const
{
	SPT_PROFILER_FUNCTION();

	const Uint32 probesToUpdateNum	= relitParams.probesNumToUpdate;
	const Uint32 raysNumPerProbe	= relitParams.raysNumPerProbe;

	const math::Vector2u probesResultRes = math::Vector2u(probesToUpdateNum, raysNumPerProbe);
	const rg::RGTextureViewHandle probesTraceResultTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Probes Trace Result"), rg::TextureDef(probesResultRes, rhi::EFragmentFormat::RGBA16_S_Float));

	const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	const AtmosphereRenderSystem& atmosphereSystem = rendererInterface.GetRenderSystemChecked<AtmosphereRenderSystem>();
	const AtmosphereContext& atmosphereContext     = atmosphereSystem.GetAtmosphereContext();

	DDGITraceRaysConsts traceRaysConsts;
	traceRaysConsts.skyViewLUT             = viewContext.skyViewLUT;
	traceRaysConsts.transmittanceLUT       = atmosphereContext.transmittanceLUT;
	traceRaysConsts.atmosphereParams       = atmosphereContext.atmosphereParams;
	traceRaysConsts.traceRaysResultTexture = probesTraceResultTexture;
	traceRaysConsts.relitParams            = relitParams.relitParams;
	traceRaysConsts.volumeParams           = relitParams.volumeParams;

	LightsRenderSystem& lightsRenderSystem = rendererInterface.GetRenderSystemChecked<LightsRenderSystem>();

	graphBuilder.TraceRays(RG_DEBUG_NAME("DDGI Trace Rays"),
						   pipelines::DDGITraceRaysPSO::pso,
						   math::Vector3u(probesToUpdateNum, relitParams.raysNumPerProbe, 1u),
						   rg::ShaderParams(traceRaysConsts,
											relitParams.ddgiGPUScene,
											lightsRenderSystem.GetGlobalLightsParams(),
											viewContext.cloudscapeProbesParams));

	return probesTraceResultTexture;
}

void DDGIRenderSystem::RenderGlobalIllumination(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "GI Update");

	if (IsDDGIEnabled())
	{
		const RelitSettings relitSettings
		{
			.reset = rendererInterface.rendererSettings.resetAccumulation
		};

		RelitScene(graphBuilder, rendererInterface, renderScene, viewSpec, relitSettings);
	}
}

DDGIRelitGPUParams DDGIRenderSystem::CreateRelitParams(const DDGIRelitZone& zone, const DDGIConfig& config) const
{
	const DDGIVolume& volume = zone.GetVolume();

	const math::AlignedBox3u& zoneProbesBoundingBox = zone.GetProbesBoundingBox();

	const Uint32 raysNumPerProbe = config.relitRaysPerProbe;

	const math::AlignedBox3f previousAABB = volume.GetPrevAABB();

	const Real32 hysteresis = ComputeRelitHysteresis(zone, config);

	DDGIRelitGPUParams params;
	params.probesToUpdateCoords     = zoneProbesBoundingBox.min();
	params.probesToUpdateCount      = zoneProbesBoundingBox.sizes();
	params.probeRaysMaxT            = 1000.f;
	params.probeRaysMinT            = 0.0f;
	params.raysNumPerProbe          = raysNumPerProbe;
	params.probesNumToUpdate        = params.probesToUpdateCount.x() * params.probesToUpdateCount.y() * params.probesToUpdateCount.z();
	params.rcpRaysNumPerProbe       = 1.f / static_cast<Real32>(params.raysNumPerProbe);
	params.rcpProbesNumToUpdate     = 1.f / static_cast<Real32>(params.probesNumToUpdate);
	params.blendHysteresis          = hysteresis;
	params.luminanceDiffThreshold   = 500.f;
	params.prevAABBMin              = previousAABB.min();
	params.prevAABBMax              = previousAABB.max();

	return params;
}

Real32 DDGIRenderSystem::ComputeRelitHysteresis(const DDGIRelitZone& zone, const DDGIConfig& config) const
{
	Real32 hysteresis = config.defaultRelitHysteresis;
	hysteresis += zone.GetRelitHysteresisDelta();
	return std::clamp(hysteresis, config.minHysteresis, config.maxHysteresis);
}

} // spt::rsc::ddgi
