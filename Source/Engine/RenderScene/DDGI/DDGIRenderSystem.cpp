#include "DDGIRenderSystem.h"
#include "RenderScene.h"
#include "DDGISceneSubsystem.h"
#include "RenderGraphBuilder.h"
#include "ResourcesManager.h"
#include "Utils/BufferUtils.h"
#include "RayTracing/RayTracingRenderSceneSubsystem.h"
#include "Common/ShaderCompilationInput.h"
#include "DescriptorSetBindings/AccelerationStructureBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/ConstantBufferRefBinding.h"
#include "DescriptorSetBindings/ConditionalBinding.h"
#include "DescriptorSetBindings/AppendConsumeBufferBinding.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "StaticMeshes/StaticMeshGeometry.h"
#include "GeometryManager.h"
#include "Lights/LightTypes.h"
#include "Shadows/ShadowMapsManagerSubsystem.h"
#include "Atmosphere/AtmosphereTypes.h"
#include "Atmosphere/AtmosphereSceneSubsystem.h"
#include "MaterialsSubsystem.h"
#include "MaterialsUnifiedData.h"
#include "Lights/LightsRenderSystem.h"
#include "DescriptorSetBindings/ChildDSBinding.h"
#include "DDGIVolume.h"
#include "Atmosphere/Clouds/VolumetricCloudsTypes.h"
#include "Pipelines/PSOsLibraryTypes.h"


namespace spt::rsc::ddgi
{

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


DS_BEGIN(DDGITraceRaysDS, rg::RGDescriptorSetState<DDGITraceRaysDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_skyViewLUT)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<AtmosphereParams>),                    u_atmosphereParams)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_transmittanceLUT)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                            u_traceRaysResultTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<DDGIRelitGPUParams>),                  u_relitParams)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<DDGIVolumeGPUParams>),                 u_volumeParams)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::ChildDSBinding<SceneRayTracingDS>),                             rayTracingDS)
DS_END();


DS_BEGIN(DDGIBlendProbesDataDS, rg::RGDescriptorSetState<DDGIBlendProbesDataDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                            u_traceRaysResultTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>), u_traceRaysResultSampler)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<DDGIRelitGPUParams>),                   u_relitParams)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<DDGIVolumeGPUParams>),                  u_volumeParams)
DS_END();


BEGIN_SHADER_STRUCT(DDGIVolumeInvalidateParams)
	SHADER_STRUCT_FIELD(math::Vector3f, prevAABBMin)
	SHADER_STRUCT_FIELD(math::Vector3f, prevAABBMax)
	SHADER_STRUCT_FIELD(Bool,           forceInvalidateAll)
END_SHADER_STRUCT();


DS_BEGIN(DDGIInvalidateProbesDS, rg::RGDescriptorSetState<DDGIInvalidateProbesDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<DDGIVolumeInvalidateParams>),                                u_invalidateParams)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<DDGIVolumeGPUParams>),                                       u_volumeParams)
	DS_BINDING(BINDING_TYPE(gfx::ArrayOfRWTexture2DBlocksBinding<math::Vector4f, constants::maxTexturesPerVolume>), u_volumeHitDistanceTextures)
	DS_BINDING(BINDING_TYPE(gfx::ArrayOfRWTexture2DBlocksBinding<math::Vector4f, constants::maxTexturesPerVolume>), u_volumeIlluminanceTextures)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture3DBinding<math::Vector4f>),                                               u_volumeProbesAverageLuminanceTexture)
DS_END();


BEGIN_SHADER_STRUCT(DDGIProbesDebugParams)
	SHADER_STRUCT_FIELD(Real32, probeRadius)
	SHADER_STRUCT_FIELD(Uint32, debugMode)
	SHADER_STRUCT_FIELD(Uint32, volumeIdx)
END_SHADER_STRUCT();


DS_BEGIN(DDGIUpdateProbesIlluminanceDS, rg::RGDescriptorSetState<DDGIUpdateProbesIlluminanceDS>)
	DS_BINDING(BINDING_TYPE(gfx::ArrayOfRWTexture2DBlocksBinding<math::Vector4f, constants::maxTexturesPerVolume>), u_volumeIlluminanceTextures)
DS_END();


DS_BEGIN(DDGIUpdateProbesHitDistanceDS, rg::RGDescriptorSetState<DDGIUpdateProbesHitDistanceDS>)
	DS_BINDING(BINDING_TYPE(gfx::ArrayOfRWTexture2DBlocksBinding<math::Vector4f, constants::maxTexturesPerVolume>), u_volumeHitDistanceTextures)
DS_END();


DS_BEGIN(DDGIUpdateProbesAverageLuminanceDS, rg::RGDescriptorSetState<DDGIUpdateProbesAverageLuminanceDS>)
	DS_BINDING(BINDING_TYPE(gfx::ArrayOfSRVTexture2DBlocksBinding<math::Vector3f, constants::maxTexturesPerVolume, true>), u_volumeIlluminanceTextures)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture3DBinding<math::Vector3f>),                                                      u_probesAverageLuminanceTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<DDGIRelitGPUParams>),                                            u_relitParams)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<DDGIVolumeGPUParams>),                                           u_volumeParams)
DS_END();


DS_BEGIN(DDGIDebugDrawProbesDS, rg::RGDescriptorSetState<DDGIDebugDrawProbesDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<DDGIProbesDebugParams>), u_ddgiProbesDebugParams)
DS_END();


DS_BEGIN(DDGIDebugDrawRaysDS, rg::RGDescriptorSetState<DDGIDebugDrawRaysDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<DDGIDebugRay>), u_debugRays)
DS_END();

//////////////////////////////////////////////////////////////////////////////////////////////////
// Pipelines =====================================================================================

namespace pipelines
{

RT_PSO(DDGITraceRaysPSO)
{
	RAY_GEN_SHADER("Sculptor/DDGI/DDGITraceRays.hlsl", DDGIProbeRaysRTG);
	MISS_SHADERS(
		SHADER_ENTRY("Sculptor/DDGI/DDGITraceRays.hlsl", DDGIProbeRaysRTM),
		SHADER_ENTRY("Sculptor/DDGI/DDGITraceRays.hlsl", DDGIShadowRaysRTM)
	);

	HIT_GROUP
	{
		CLOSEST_HIT_SHADER("Sculptor/StaticMeshes/SMDDGI.hlsl", DDGI_RT_CHS);
		ANY_HIT_SHADER("Sculptor/StaticMeshes/SMDDGI.hlsl", DDGI_RT_AHS);

		HIT_PERMUTATION_DOMAIN(mat::RTHitGroupPermutation);
	};

	PRESET(pso);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		const lib::DynamicArray<mat::RTHitGroupPermutation> materialPermutations = mat::MaterialsSubsystem::Get().GetRTHitGroups();

		lib::DynamicArray<HitGroup> hitGroups;
		hitGroups.reserve(materialPermutations.size());

		for (const mat::RTHitGroupPermutation& permutation : materialPermutations)
		{
			HitGroup hitGroup;
			hitGroup.permutation = permutation;
			hitGroups.push_back(std::move(hitGroup));
		}

		const rhi::RayTracingPipelineDefinition psoDefinition{ .maxRayRecursionDepth = 1u };

		pso = CompilePSO(compiler, psoDefinition, hitGroups);
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

DDGIVolumeRelitParameters::DDGIVolumeRelitParameters(rg::RenderGraphBuilder& graphBuilder, const DDGIRelitGPUParams& relitParams, const DDGISceneSubsystem& ddgiSubsystem, const DDGIVolume& inVolume)
	: volume(inVolume)
	, probesAverageLuminanceTextureView(graphBuilder.AcquireExternalTextureView(volume.GetProbesAverageLuminanceTexture()))
	, relitParamsBuffer(rdr::utils::CreateConstantBufferView<DDGIRelitGPUParams>(RENDERER_RESOURCE_NAME("DDGIRelitGPUParams"), relitParams))
	, ddgiVolumeParamsBuffer(rdr::utils::CreateConstantBufferView<DDGIVolumeGPUParams>(RENDERER_RESOURCE_NAME("DDGIVolumeGPUParams"), volume.GetVolumeGPUParams()))
	, probesNumToUpdate(relitParams.probesNumToUpdate)
	, raysNumPerProbe(relitParams.raysNumPerProbe)
	, probeIlluminanceDataWithBorderRes(volume.GetVolumeGPUParams().probeIlluminanceDataWithBorderRes)
	, probeHitDistanceDataWithBorderRes(volume.GetVolumeGPUParams().probeHitDistanceDataWithBorderRes)
	, ddgiSceneDS(ddgiSubsystem.GetDDGISceneDS())
	, debugMode(ddgiSubsystem.GetDebugMode())
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

DDGIRenderSystem::DDGIRenderSystem()
{
	m_supportedStages = lib::Flags(ERenderStage::GlobalIllumination, ERenderStage::DeferredShading);
}

void DDGIRenderSystem::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings)
{
	SPT_PROFILER_FUNCTION();

	Super::RenderPerFrame(graphBuilder, renderScene, viewSpecs, settings);

	DDGISceneSubsystem& ddgiSubsystem = renderScene.GetSceneSubsystemChecked<DDGISceneSubsystem>();
	ddgiSubsystem.UpdateDDGIScene(viewSpecs[0]->GetRenderView());

	for (ViewRenderingSpec* view : viewSpecs)
	{
		SPT_CHECK(!!view);
		RenderPerView(graphBuilder, renderScene, *view);
	}
}

void DDGIRenderSystem::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	if (viewSpec.SupportsStage(ERenderStage::GlobalIllumination))
	{
		viewSpec.GetRenderStageEntries(ERenderStage::GlobalIllumination).GetOnRenderStage().AddRawMember(this, &DDGIRenderSystem::RenderGlobalIllumination);
	}
}

void DDGIRenderSystem::RelitScene(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const DDGISceneSubsystem& ddgiSubsystem, DDGIScene& scene, const RelitSettings& settings) const
{
	SPT_PROFILER_FUNCTION();

	InvalidateVolumes(graphBuilder, renderScene, viewSpec, ddgiSubsystem, scene, settings);

	const SizeType relitBudget = settings.reset ? maxValue<SizeType> : ddgiSubsystem.GetConfig().relitVolumesBudget;

	DDGIZonesCollector zonesCollector(relitBudget);

	scene.CollectZonesToRelit(zonesCollector);

	const lib::DynamicArray<DDGIRelitZone*>& zones = zonesCollector.GetZonesToRelit();

	for (DDGIRelitZone* zone : zones)
	{
		SPT_CHECK(!!zone);

		RelitZone(graphBuilder, renderScene, viewSpec, ddgiSubsystem, *zone);
	}

	scene.PostRelit();
}

void DDGIRenderSystem::InvalidateVolumes(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const DDGISceneSubsystem& ddgiSubsystem, DDGIScene& scene, const RelitSettings& settings) const
{
	SPT_PROFILER_FUNCTION();

	const lib::DynamicArray<DDGIVolume*>& volumes = scene.GetVolumes();

	for (DDGIVolume* volume : volumes)
	{
		SPT_CHECK(!!volume);

		if (volume->RequiresInvalidation() || settings.reset)
		{
			InvalidateOutOfBoundsProbes(graphBuilder, renderScene, viewSpec, *volume, settings);
		}
	}
}

void DDGIRenderSystem::RelitZone(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const DDGISceneSubsystem& ddgiSubsystem, DDGIRelitZone& zone) const
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "Relit Zone");

	const DDGIVolumeRelitParameters relitParams(graphBuilder, CreateRelitParams(zone, ddgiSubsystem.GetConfig()), ddgiSubsystem, zone.GetVolume());

	UpdateProbes(graphBuilder, renderScene, viewSpec, relitParams);

	zone.PostRelit();
}

void DDGIRenderSystem::UpdateProbes(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const DDGIVolumeRelitParameters& relitParams) const
{
	SPT_PROFILER_FUNCTION();

	const rg::RGTextureViewHandle probesTraceResultTexture = TraceRays(graphBuilder, renderScene, viewSpec, relitParams);

	const lib::MTHandle<DDGIBlendProbesDataDS> updateProbesDS = graphBuilder.CreateDescriptorSet<DDGIBlendProbesDataDS>(RENDERER_RESOURCE_NAME("DDGIBlendProbesDataDS"));
	updateProbesDS->u_traceRaysResultTexture = probesTraceResultTexture;
	updateProbesDS->u_relitParams            = relitParams.relitParamsBuffer;
	updateProbesDS->u_volumeParams           = relitParams.ddgiVolumeParamsBuffer;

	lib::MTHandle<DDGIUpdateProbesIlluminanceDS> updateProbesIlluminanceDS = graphBuilder.CreateDescriptorSet<DDGIUpdateProbesIlluminanceDS>(RENDERER_RESOURCE_NAME("DDGIUpdateProbesIlluminanceDS"));
	{
		const gfx::TexturesBindingsAllocationHandle illuminanceTexturesBlock = updateProbesIlluminanceDS->u_volumeIlluminanceTextures.AllocateWholeBlock();
		updateProbesIlluminanceDS->u_volumeIlluminanceTextures.BindTextures(relitParams.probesIlluminanceTextureViews, illuminanceTexturesBlock, 0);
	}

	const rdr::PipelineStateID updateProbesIlluminancePipelineID = pipelines::CreateDDGIBlendProbesIlluminancePipeline(relitParams.probeIlluminanceDataWithBorderRes, relitParams.raysNumPerProbe);
	
	graphBuilder.Dispatch(RG_DEBUG_NAME("DDGI Update Probes Illuminance"),
						  updateProbesIlluminancePipelineID,
						  math::Vector3u(relitParams.probesNumToUpdate, 1u, 1u),
						  rg::BindDescriptorSets(updateProbesDS,
												 std::move(updateProbesIlluminanceDS)));

	lib::MTHandle<DDGIUpdateProbesHitDistanceDS> updateProbesHitDistanceDS = graphBuilder.CreateDescriptorSet<DDGIUpdateProbesHitDistanceDS>(RENDERER_RESOURCE_NAME("DDGIUpdateProbesHitDistanceDS"));
	{
		const gfx::TexturesBindingsAllocationHandle hitDistanceTexturesBlock = updateProbesHitDistanceDS->u_volumeHitDistanceTextures.AllocateWholeBlock();
		updateProbesHitDistanceDS->u_volumeHitDistanceTextures.BindTextures(relitParams.probesHitDistanceTextureViews, hitDistanceTexturesBlock, 0);
	}
	
	const rdr::PipelineStateID updateProbesDistancesPipelineID = pipelines::CreateDDGIBlendProbesHitDistancePipeline(relitParams.probeHitDistanceDataWithBorderRes, relitParams.raysNumPerProbe);;

	graphBuilder.Dispatch(RG_DEBUG_NAME("DDGI Update Probes Hit Distance"),
						  updateProbesDistancesPipelineID,
						  math::Vector3u(relitParams.probesNumToUpdate, 1u, 1u),
						  rg::BindDescriptorSets(updateProbesDS,
												 std::move(updateProbesHitDistanceDS)));

	lib::MTHandle<DDGIUpdateProbesAverageLuminanceDS> updateProbesAverageLuminanceDS = graphBuilder.CreateDescriptorSet<DDGIUpdateProbesAverageLuminanceDS>(RENDERER_RESOURCE_NAME("DDGIUpdateProbesAverageLuminanceDS"));
	updateProbesAverageLuminanceDS->u_probesAverageLuminanceTexture = relitParams.probesAverageLuminanceTextureView;
	updateProbesAverageLuminanceDS->u_relitParams                   = relitParams.relitParamsBuffer;
	updateProbesAverageLuminanceDS->u_volumeParams                  = relitParams.ddgiVolumeParamsBuffer;

	{
		const gfx::TexturesBindingsAllocationHandle illuminanceTexturesBlock = updateProbesAverageLuminanceDS->u_volumeIlluminanceTextures.AllocateWholeBlock();
		updateProbesAverageLuminanceDS->u_volumeIlluminanceTextures.BindTextures(relitParams.probesIlluminanceTextureViews, illuminanceTexturesBlock, 0);
	}

	const rdr::PipelineStateID updateProbesAverageLuminancePipelineID = pipelines::CreateDDGIUpdateProbesAverageLuminancePipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("DDGI Update Probes Average Luminance"),
						  updateProbesAverageLuminancePipelineID,
						  math::Vector3u(relitParams.probesNumToUpdate, 1u, 1u),
						  rg::BindDescriptorSets(std::move(updateProbesAverageLuminanceDS)));
}

void DDGIRenderSystem::InvalidateOutOfBoundsProbes(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, DDGIVolume& volume, const RelitSettings& settings) const
{
	SPT_PROFILER_FUNCTION();

	const math::Vector3u volumeRes = volume.GetProbesVolumeResolution();

	const DDGIVolumeGPUParams& volumeParams = volume.GetVolumeGPUParams();

	const math::AlignedBox3f prevAABB = volume.GetPrevAABB();

	DDGIVolumeInvalidateParams invalidateParams;
	invalidateParams.prevAABBMin        = prevAABB.min();
	invalidateParams.prevAABBMax        = prevAABB.max();
	invalidateParams.forceInvalidateAll = settings.reset;

	const lib::MTHandle<DDGIInvalidateProbesDS> invalidateProbesDS = graphBuilder.CreateDescriptorSet<DDGIInvalidateProbesDS>(RENDERER_RESOURCE_NAME("DDGIInvalidateProbesDS"));
	invalidateProbesDS->u_invalidateParams                    = invalidateParams;
	invalidateProbesDS->u_volumeParams                        = volumeParams;
	invalidateProbesDS->u_volumeProbesAverageLuminanceTexture = volume.GetProbesAverageLuminanceTexture();

	const Uint32 probeDataTexturesNum = volume.GetProbeDataTexturesNum();

	const gfx::TexturesBindingsAllocationHandle illuminanceTexturesBlock = invalidateProbesDS->u_volumeIlluminanceTextures.AllocateWholeBlock();
	for (Uint32 textureIdx = 0u; textureIdx < probeDataTexturesNum; ++textureIdx)
	{
		const rg::RGTextureViewHandle textureView = graphBuilder.AcquireExternalTextureView(volume.GetProbesIlluminanceTexture(textureIdx));
		invalidateProbesDS->u_volumeIlluminanceTextures.BindTexture(textureView, illuminanceTexturesBlock, textureIdx);
	}
	const gfx::TexturesBindingsAllocationHandle hitDistanceTexturesBlock = invalidateProbesDS->u_volumeHitDistanceTextures.AllocateWholeBlock();
	for (Uint32 textureIdx = 0u; textureIdx < probeDataTexturesNum; ++textureIdx)
	{
		const rg::RGTextureViewHandle textureView = graphBuilder.AcquireExternalTextureView(volume.GetProbesHitDistanceTexture(textureIdx));
		invalidateProbesDS->u_volumeHitDistanceTextures.BindTexture(textureView, hitDistanceTexturesBlock, textureIdx);
	}

	static const rdr::PipelineStateID invalidateProbesPipelineID = pipelines::CreateDDGIInvalidateProbesPipeline(volumeParams.probeHitDistanceDataWithBorderRes);

	graphBuilder.Dispatch(RG_DEBUG_NAME("DDGI Invalidate Probes"),
						  invalidateProbesPipelineID,
						  volumeRes,
						  rg::BindDescriptorSets(invalidateProbesDS));

	volume.PostInvalidation();
}

rg::RGTextureViewHandle DDGIRenderSystem::TraceRays(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const DDGIVolumeRelitParameters& relitParams) const
{
	SPT_PROFILER_FUNCTION();

	const Uint32 probesToUpdateNum	= relitParams.probesNumToUpdate;
	const Uint32 raysNumPerProbe	= relitParams.raysNumPerProbe;

	const math::Vector2u probesResultRes = math::Vector2u(probesToUpdateNum, raysNumPerProbe);
	const rg::RGTextureViewHandle probesTraceResultTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Probes Trace Result"), rg::TextureDef(probesResultRes, rhi::EFragmentFormat::RGBA16_S_Float));

	const RayTracingRenderSceneSubsystem& rayTracingSubsystem = renderScene.GetSceneSubsystemChecked<RayTracingRenderSceneSubsystem>();

	const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	const AtmosphereSceneSubsystem& atmosphereSubsystem = renderScene.GetSceneSubsystemChecked<AtmosphereSceneSubsystem>();
	const AtmosphereContext& atmosphereContext = atmosphereSubsystem.GetAtmosphereContext();

	const lib::MTHandle<DDGITraceRaysDS> traceRaysDS = graphBuilder.CreateDescriptorSet<DDGITraceRaysDS>(RENDERER_RESOURCE_NAME("DDGITraceRaysDS"));
	traceRaysDS->u_skyViewLUT             = viewContext.skyViewLUT;
	traceRaysDS->u_transmittanceLUT       = atmosphereContext.transmittanceLUT;
	traceRaysDS->u_atmosphereParams       = atmosphereContext.atmosphereParamsBuffer->GetFullView();
	traceRaysDS->u_traceRaysResultTexture = probesTraceResultTexture;
	traceRaysDS->u_relitParams            = relitParams.relitParamsBuffer;
	traceRaysDS->u_volumeParams           = relitParams.ddgiVolumeParamsBuffer;
	traceRaysDS->rayTracingDS             = rayTracingSubsystem.GetSceneRayTracingDS();

	LightsRenderSystem& lightsRenderSystem = renderScene.GetRenderSystemChecked<LightsRenderSystem>();

	lib::MTHandle<ShadowMapsDS> shadowMapsDS;
	
	const lib::SharedPtr<ShadowMapsManagerSubsystem> shadowMapsManager = renderScene.GetSceneSubsystem<ShadowMapsManagerSubsystem>();
	if (shadowMapsManager && shadowMapsManager->CanRenderShadows())
	{
		shadowMapsDS = shadowMapsManager->GetShadowMapsDS();
	}
	else
	{
		shadowMapsDS = graphBuilder.CreateDescriptorSet<ShadowMapsDS>(RENDERER_RESOURCE_NAME("DDGI Shadow Maps DS"));
	}

	graphBuilder.TraceRays(RG_DEBUG_NAME("DDGI Trace Rays"),
						   pipelines::DDGITraceRaysPSO::pso,
						   math::Vector3u(probesToUpdateNum, relitParams.raysNumPerProbe, 1u),
						   rg::BindDescriptorSets(traceRaysDS,
												  relitParams.ddgiSceneDS,
												  lightsRenderSystem.GetGlobalLightsDS(),
												  StaticMeshUnifiedData::Get().GetUnifiedDataDS(),
												  GeometryManager::Get().GetGeometryDSState(),
												  mat::MaterialsUnifiedData::Get().GetMaterialsDS(),
												  viewContext.cloudscapeProbesDS,
												  shadowMapsDS));

	return probesTraceResultTexture;
}

void DDGIRenderSystem::RenderGlobalIllumination(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context, RenderStageContextMetaDataHandle metaData) const
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "GI Update");

	DDGISceneSubsystem& ddgiSubsystem = renderScene.GetSceneSubsystemChecked<DDGISceneSubsystem>();

	if (ddgiSubsystem.IsDDGIEnabled())
	{
		const RelitSettings relitSettings
		{
			.reset = context.rendererSettings.resetAccumulation
		};

		RelitScene(graphBuilder, renderScene, viewSpec, ddgiSubsystem, ddgiSubsystem.GetDDGIScene(), relitSettings);
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
	params.probeRaysMaxT            = 100.f;
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
