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
#include "SceneRenderer/SceneRendererTypes.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "StaticMeshes/StaticMeshGeometry.h"
#include "GeometryManager.h"
#include "Lights/LightTypes.h"
#include "Shadows/ShadowMapsManagerSubsystem.h"
#include "Atmosphere/AtmosphereTypes.h"
#include "Atmosphere/AtmosphereSceneSubsystem.h"
#include "MaterialsSubsystem.h"
#include "MaterialsUnifiedData.h"

namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Descriptor Sets ===============================================================================

BEGIN_SHADER_STRUCT(DDGIBlendParams)
	SHADER_STRUCT_FIELD(math::Vector2f, traceRaysResultTexturePixelSize)
END_SHADER_STRUCT();


DS_BEGIN(DDGITraceRaysDS, rg::RGDescriptorSetState<DDGITraceRaysDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),								u_skyViewLUT)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<AtmosphereParams>),						u_atmosphereParams)
	DS_BINDING(BINDING_TYPE(gfx::AccelerationStructureBinding),										u_sceneAccelerationStructure)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<RTInstanceData>),							u_rtInstances)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),								u_traceRaysResultTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<DDGIUpdateProbesGPUParams>),				u_updateProbesParams)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<DDGIGPUParams>),							u_ddgiParams)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),								u_probesIlluminanceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),								u_probesHitDistanceTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),	u_linearSampler)
DS_END();


DS_BEGIN(DDGIBlendProbesDataDS, rg::RGDescriptorSetState<DDGIBlendProbesDataDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),								u_traceRaysResultTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_traceRaysResultSampler)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<DDGIUpdateProbesGPUParams>),				u_updateProbesParams)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<DDGIGPUParams>),							u_ddgiParams)
DS_END();


BEGIN_SHADER_STRUCT(DDGILightsParams)
	SHADER_STRUCT_FIELD(Uint32, pointLightsNum)
	SHADER_STRUCT_FIELD(Uint32, directionalLightsNum)
END_SHADER_STRUCT();


DS_BEGIN(DDGILightsDS, rg::RGDescriptorSetState<DDGILightsDS>)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<DDGILightsParams>),		u_lightsParams)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<PointLightGPUData>),			u_pointLights)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<DirectionalLightGPUData>),		u_directionalLights)
DS_END();


BEGIN_SHADER_STRUCT(DDGIProbesDebugParams)
	SHADER_STRUCT_FIELD(Real32, probeRadius)
	SHADER_STRUCT_FIELD(Uint32, debugMode)
END_SHADER_STRUCT();


DS_BEGIN(DDGIUpdateProbesIlluminanceDS, rg::RGDescriptorSetState<DDGIUpdateProbesIlluminanceDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>), u_probesIlluminanceTexture)
DS_END();


DS_BEGIN(DDGIUpdateProbesHitDistanceDS, rg::RGDescriptorSetState<DDGIUpdateProbesHitDistanceDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector2f>), u_probesHitDistanceTexture)
DS_END();


DS_BEGIN(DDGIDebugDrawProbesDS, rg::RGDescriptorSetState<DDGIDebugDrawProbesDS>)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<DDGIProbesDebugParams>),			u_ddgiProbesDebugParams)
DS_END();

//////////////////////////////////////////////////////////////////////////////////////////////////
// Pipelines =====================================================================================

namespace pipelines
{

static rdr::PipelineStateID CreateDDGITraceRaysPipeline(const RayTracingRenderSceneSubsystem& rayTracingSubsystem)
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.DisableGeneratingDebugSource();

	rdr::RayTracingPipelineShaders rtShaders;

	rtShaders.rayGenShader = rdr::ResourcesManager::CreateShader("Sculptor/DDGI/DDGITraceRays.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::RTGeneration, "DDGIProbeRaysRTG"), compilationSettings);
	rtShaders.missShaders.emplace_back(rdr::ResourcesManager::CreateShader("Sculptor/DDGI/DDGITraceRays.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::RTMiss, "DDGIProbeRaysRTM"), compilationSettings));
	rtShaders.missShaders.emplace_back(rdr::ResourcesManager::CreateShader("Sculptor/DDGI/DDGITraceRays.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::RTMiss, "DDGIShadowRaysRTM"), compilationSettings));

	const lib::DynamicArray<mat::MaterialShadersHash>& sbtRecords = rayTracingSubsystem.GetMaterialShaderSBTRecords();

	for (SizeType recordIdx = 0; recordIdx < sbtRecords.size(); ++recordIdx)
	{
		const mat::MaterialRayTracingShaders shaders = mat::MaterialsSubsystem::Get().GetMaterialShaders<mat::MaterialRayTracingShaders>("DDGI", sbtRecords[recordIdx]);

		rdr::RayTracingHitGroup hitGroup;
		hitGroup.closestHitShader	= shaders.closestHitShader;
		hitGroup.anyHitShader		= shaders.anyHitShader;

		rtShaders.hitGroups.emplace_back(hitGroup);
	}

	rhi::RayTracingPipelineDefinition pipelineDefinition;
	pipelineDefinition.maxRayRecursionDepth = 1;
	return rdr::ResourcesManager::CreateRayTracingPipeline(RENDERER_RESOURCE_NAME("DDGI Trace Rays Pipeline"), rtShaders, pipelineDefinition);
}

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

static rdr::PipelineStateID CreateDDGIDrawDebugProbesPipeline(rhi::EFragmentFormat colorRTFormat, rhi::EFragmentFormat depthFormat)
{
	rdr::GraphicsPipelineShaders shaders;
	shaders.vertexShader = rdr::ResourcesManager::CreateShader("Sculptor/DDGI/DDGIDebugProbes.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Vertex, "DDGIDebugProbesVS"));
	shaders.fragmentShader = rdr::ResourcesManager::CreateShader("Sculptor/DDGI/DDGIDebugProbes.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Fragment, "DDGIDebugProbesPS"));

	rhi::GraphicsPipelineDefinition pipelineDef;
	pipelineDef.primitiveTopology = rhi::EPrimitiveTopology::TriangleList;
	pipelineDef.renderTargetsDefinition.depthRTDefinition.format = depthFormat;
	pipelineDef.renderTargetsDefinition.depthRTDefinition.depthCompareOp = spt::rhi::ECompareOp::GreaterOrEqual;
	pipelineDef.renderTargetsDefinition.colorRTsDefinition.emplace_back(rhi::ColorRenderTargetDefinition(colorRTFormat));

	return rdr::ResourcesManager::CreateGfxPipeline(RENDERER_RESOURCE_NAME("DDGI Debug Probes Pipeline"), shaders, pipelineDef);
}

} // pipelines

namespace utils
{

static lib::SharedRef<DDGILightsDS> CreateGlobalLightsDS(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene)
{
	const RenderSceneRegistry& sceneRegistry = renderScene.GetRegistry();

	const auto pointLightsView = sceneRegistry.view<const PointLightData>();
	const SizeType pointLightsNum = pointLightsView.size();

	lib::SharedPtr<rdr::Buffer> pointLightsBuffer;
	if (pointLightsNum > 0)
	{
		const Uint64 pointLightsDataSize = pointLightsNum * sizeof(PointLightGPUData);
		const rhi::BufferDefinition pointLightsDataBufferDef(pointLightsDataSize, rhi::EBufferUsage::Storage);
		pointLightsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("DDGI Point Lights Buffer"), pointLightsDataBufferDef, rhi::EMemoryUsage::CPUToGPU);
		rhi::RHIMappedBuffer<PointLightGPUData> pointLightsData(pointLightsBuffer->GetRHI());

		SizeType pointLightIdx = 0;

		for (const auto& [entity, pointLight] : pointLightsView.each())
		{
			PointLightGPUData& gpuLightData = pointLightsData[pointLightIdx++];
			gpuLightData = pointLight.GenerateGPUData();
			gpuLightData.entityID = static_cast<Uint32>(entity);
			gpuLightData.shadowMapFirstFaceIdx = idxNone<Uint32>;

			if (const PointLightShadowMapComponent* shadowMapComp = sceneRegistry.try_get<const PointLightShadowMapComponent>(entity))
			{
				gpuLightData.shadowMapFirstFaceIdx = shadowMapComp->shadowMapFirstFaceIdx;
				SPT_CHECK(gpuLightData.shadowMapFirstFaceIdx != idxNone<Uint32>);
			}
		}
	}

	const auto directionalLightsView = sceneRegistry.view<const DirectionalLightData>();
	const SizeType directionalLightsNum = directionalLightsView.size();

	lib::SharedPtr<rdr::Buffer> directionalLightsBuffer;
	if (directionalLightsNum > 0)
	{
		const Uint64 directionalLightsDataSize = directionalLightsNum * sizeof(DirectionalLightGPUData);
		const rhi::BufferDefinition directionalLightsDataBufferDef(directionalLightsDataSize, rhi::EBufferUsage::Storage);
		directionalLightsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("DDGI Directional Lights Buffer"), directionalLightsDataBufferDef, rhi::EMemoryUsage::CPUToGPU);
		rhi::RHIMappedBuffer<DirectionalLightGPUData> directionalLightsData(directionalLightsBuffer->GetRHI());

		SizeType directionalLightIdx = 0;


		for (const auto& [entity, directionalLight] : directionalLightsView.each())
		{
			DirectionalLightGPUData& lightGPUData = directionalLightsData[directionalLightIdx++];
			lightGPUData = directionalLight.GenerateGPUData();
			lightGPUData.shadowMaskIdx = idxNone<Uint32>;
		}
	}

	DDGILightsParams lightsParams;
	lightsParams.pointLightsNum			= static_cast<Uint32>(pointLightsNum);
	lightsParams.directionalLightsNum	= static_cast<Uint32>(directionalLightsNum);

	const lib::SharedRef<DDGILightsDS> lightsDS = rdr::ResourcesManager::CreateDescriptorSetState<DDGILightsDS>(RENDERER_RESOURCE_NAME("DDGILightsDS"));
	lightsDS->u_lightsParams		= lightsParams;
	lightsDS->u_pointLights			= pointLightsBuffer->CreateFullView();
	lightsDS->u_directionalLights	= directionalLightsBuffer->CreateFullView();

	return lightsDS;
}

} // utils

//////////////////////////////////////////////////////////////////////////////////////////////////
// DDGIUpdateParameters ==========================================================================

DDGIUpdateParameters::DDGIUpdateParameters(const DDGIUpdateProbesGPUParams& gpuUpdateParams, const DDGISceneSubsystem& ddgiSubsystem)
	: probesIlluminanceTextureView(ddgiSubsystem.GetProbesIlluminanceTexture())
	, probesHitDistanceTextureView(ddgiSubsystem.GetProbesHitDistanceTexture())
	, updateProbesParamsBuffer(rdr::utils::CreateConstantBufferView<DDGIUpdateProbesGPUParams>(RENDERER_RESOURCE_NAME("DDGIUpdateProbesGPUParams"), gpuUpdateParams))
	, ddgiParamsBuffer(rdr::utils::CreateConstantBufferView<DDGIGPUParams>(RENDERER_RESOURCE_NAME("DDGIGPUParams"), ddgiSubsystem.GetDDGIParams()))
	, probesNumToUpdate(gpuUpdateParams.probesNumToUpdate)
	, raysNumPerProbe(gpuUpdateParams.raysNumPerProbe)
	, probeIlluminanceDataWithBorderRes(ddgiSubsystem.GetDDGIParams().probeIlluminanceDataWithBorderRes)
	, probeHitDistanceDataWithBorderRes(ddgiSubsystem.GetDDGIParams().probeHitDistanceDataWithBorderRes)
{ }

//////////////////////////////////////////////////////////////////////////////////////////////////
// DDGIRenderSystem ==============================================================================

DDGIRenderSystem::DDGIRenderSystem()
{
	m_supportedStages = lib::Flags(ERenderStage::GlobalIllumination, ERenderStage::ForwardOpaque);
}

void DDGIRenderSystem::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	Super::RenderPerView(graphBuilder, renderScene, viewSpec);

	const DDGISceneSubsystem& ddgiSubsystem = renderScene.GetSceneSubsystemChecked<DDGISceneSubsystem>();

	if (viewSpec.SupportsStage(ERenderStage::GlobalIllumination))
	{
		viewSpec.GetRenderStageEntries(ERenderStage::GlobalIllumination).GetOnRenderStage().AddRawMember(this, &DDGIRenderSystem::RenderGlobalIllumination);
	}

	if (viewSpec.SupportsStage(ERenderStage::ForwardOpaque))
	{
		const EDDDGIProbesDebugMode::Type probesDebug = ddgiSubsystem.GetProbesDebugMode();
		if (probesDebug != EDDDGIProbesDebugMode::None)
		{
			viewSpec.GetRenderViewEntry(RenderViewEntryDelegates::RenderSceneDebugLayer).AddRawMember(this, &DDGIRenderSystem::RenderDebugProbes);
		}
	}
}

void DDGIRenderSystem::UpdateProbes(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const DDGIUpdateParameters& updateParams) const
{
	SPT_PROFILER_FUNCTION();

	const rg::RGTextureViewHandle probesTraceResultTexture = TraceRays(graphBuilder, renderScene, viewSpec, updateParams);

	const lib::SharedRef<DDGIBlendProbesDataDS> updateProbesDS = rdr::ResourcesManager::CreateDescriptorSetState<DDGIBlendProbesDataDS>(RENDERER_RESOURCE_NAME("DDGIBlendProbesDataDS"));
	updateProbesDS->u_traceRaysResultTexture	= probesTraceResultTexture;
	updateProbesDS->u_updateProbesParams		= updateParams.updateProbesParamsBuffer;
	updateProbesDS->u_ddgiParams				= updateParams.ddgiParamsBuffer;

	lib::SharedPtr<DDGIUpdateProbesIlluminanceDS> updateProbesIlluminanceDS = rdr::ResourcesManager::CreateDescriptorSetState<DDGIUpdateProbesIlluminanceDS>(RENDERER_RESOURCE_NAME("DDGIUpdateProbesIlluminanceDS"));
	updateProbesIlluminanceDS->u_probesIlluminanceTexture = updateParams.probesIlluminanceTextureView;

	const rdr::PipelineStateID updateProbesIlluminancePipelineID = pipelines::CreateDDGIBlendProbesIlluminancePipeline(updateParams.probeIlluminanceDataWithBorderRes, updateParams.raysNumPerProbe);
	
	graphBuilder.Dispatch(RG_DEBUG_NAME("DDGI Update Probes Illuminance"),
						  updateProbesIlluminancePipelineID,
						  math::Vector3u(updateParams.probesNumToUpdate, 1u, 1u),
						  rg::BindDescriptorSets(updateProbesDS,
												 std::move(updateProbesIlluminanceDS)));

	lib::SharedPtr<DDGIUpdateProbesHitDistanceDS> updateProbesHitDistanceDS = rdr::ResourcesManager::CreateDescriptorSetState<DDGIUpdateProbesHitDistanceDS>(RENDERER_RESOURCE_NAME("DDGIUpdateProbesHitDistanceDS"));
	updateProbesHitDistanceDS->u_probesHitDistanceTexture = updateParams.probesHitDistanceTextureView;
	
	const rdr::PipelineStateID updateProbesDistancesPipelineID = pipelines::CreateDDGIBlendProbesHitDistancePipeline(updateParams.probeHitDistanceDataWithBorderRes, updateParams.raysNumPerProbe);;

	graphBuilder.Dispatch(RG_DEBUG_NAME("DDGI Update Probes Hit Distance"),
						  updateProbesDistancesPipelineID,
						  math::Vector3u(updateParams.probesNumToUpdate, 1u, 1u),
						  rg::BindDescriptorSets(updateProbesDS,
												 std::move(updateProbesHitDistanceDS)));
}

rg::RGTextureViewHandle DDGIRenderSystem::TraceRays(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const DDGIUpdateParameters& updateParams) const
{
	SPT_PROFILER_FUNCTION();

	const Uint32 probesToUpdateNum	= updateParams.probesNumToUpdate;
	const Uint32 raysNumPerProbe	= updateParams.raysNumPerProbe;

	rhi::TextureDefinition probesTraceResultTextureDef;
	probesTraceResultTextureDef.resolution	= math::Vector3u(probesToUpdateNum, raysNumPerProbe, 1u);
	probesTraceResultTextureDef.usage		= lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture);
	probesTraceResultTextureDef.format		= rhi::EFragmentFormat::RGBA16_S_Float;
	const rg::RGTextureViewHandle probesTraceResultTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Probes Trace Result"), probesTraceResultTextureDef, rhi::EMemoryUsage::GPUOnly);

	const RayTracingRenderSceneSubsystem& rayTracingSubsystem = renderScene.GetSceneSubsystemChecked<RayTracingRenderSceneSubsystem>();

	const ViewAtmosphereRenderData& viewAtmosphereData = viewSpec.GetData().Get<ViewAtmosphereRenderData>();
	SPT_CHECK(viewAtmosphereData.skyViewLUT.IsValid());

	const AtmosphereSceneSubsystem& atmosphereSubsystem = renderScene.GetSceneSubsystemChecked<AtmosphereSceneSubsystem>();
	const AtmosphereContext& atmosphereContext = atmosphereSubsystem.GetAtmosphereContext();

	const lib::SharedPtr<DDGITraceRaysDS> traceRaysDS = rdr::ResourcesManager::CreateDescriptorSetState<DDGITraceRaysDS>(RENDERER_RESOURCE_NAME("DDGITraceRaysDS"));
	traceRaysDS->u_skyViewLUT					= viewAtmosphereData.skyViewLUT;
	traceRaysDS->u_atmosphereParams				= atmosphereContext.atmosphereParamsBuffer->CreateFullView();
	traceRaysDS->u_sceneAccelerationStructure	= lib::Ref(rayTracingSubsystem.GetSceneTLAS());
	traceRaysDS->u_rtInstances					= rayTracingSubsystem.GetRTInstancesDataBuffer()->CreateFullView();
	traceRaysDS->u_traceRaysResultTexture		= probesTraceResultTexture;
	traceRaysDS->u_updateProbesParams			= updateParams.updateProbesParamsBuffer;
	traceRaysDS->u_ddgiParams					= updateParams.ddgiParamsBuffer;
	traceRaysDS->u_probesIlluminanceTexture		= updateParams.probesIlluminanceTextureView;
	traceRaysDS->u_probesHitDistanceTexture		= updateParams.probesHitDistanceTextureView;

	const lib::SharedRef<DDGILightsDS> lightsDS = utils::CreateGlobalLightsDS(graphBuilder, renderScene);

	lib::SharedPtr<ShadowMapsDS> shadowMapsDS;
	
	const lib::SharedPtr<ShadowMapsManagerSubsystem> shadowMapsManager = renderScene.GetSceneSubsystem<ShadowMapsManagerSubsystem>();
	if (shadowMapsManager && shadowMapsManager->CanRenderShadows())
	{
		shadowMapsDS = shadowMapsManager->GetShadowMapsDS();
	}
	else
	{
		shadowMapsDS = rdr::ResourcesManager::CreateDescriptorSetState<ShadowMapsDS>(RENDERER_RESOURCE_NAME("DDGI Shadow Maps DS"));
	}

	static rdr::PipelineStateID traceRaysPipelineID;
	if (!traceRaysPipelineID.IsValid() || rayTracingSubsystem.AreSBTRecordsDirty())
	{
		traceRaysPipelineID = pipelines::CreateDDGITraceRaysPipeline(rayTracingSubsystem);
	}

	graphBuilder.TraceRays(RG_DEBUG_NAME("DDGI Trace Rays"),
						   traceRaysPipelineID,
						   math::Vector3u(probesToUpdateNum, updateParams.raysNumPerProbe, 1u),
						   rg::BindDescriptorSets(traceRaysDS,
												  lightsDS,
												  StaticMeshUnifiedData::Get().GetUnifiedDataDS(),
												  GeometryManager::Get().GetGeometryDSState(),
												  mat::MaterialsUnifiedData::Get().GetMaterialsDS(),
												  shadowMapsDS));

	return probesTraceResultTexture;
}

void DDGIRenderSystem::RenderGlobalIllumination(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context) const
{
	SPT_PROFILER_FUNCTION();

	DDGISceneSubsystem& ddgiSubsystem = scene.GetSceneSubsystemChecked<DDGISceneSubsystem>();

	if (ddgiSubsystem.IsDDGIEnabled())
	{
		if (ddgiSubsystem.RequiresClearingData())
		{
			const rg::RGTextureViewHandle probesIlluminanceTextureView = graphBuilder.AcquireExternalTextureView(ddgiSubsystem.GetProbesIlluminanceTexture());

			graphBuilder.ClearTexture(RG_DEBUG_NAME("Clear DDGI Data"), probesIlluminanceTextureView, rhi::ClearColor());

			ddgiSubsystem.PostClearingData();
		}

		const DDGIUpdateParameters updateParams(ddgiSubsystem.CreateUpdateProbesParams(), ddgiSubsystem);

		UpdateProbes(graphBuilder, scene, viewSpec, updateParams);

		ddgiSubsystem.PostUpdateProbes();
	}
}

void DDGIRenderSystem::RenderDebugProbes(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context) const
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const DDGISceneSubsystem& ddgiSubsystem = renderScene.GetSceneSubsystemChecked<DDGISceneSubsystem>();

	DDGIProbesDebugParams debugParams;
	debugParams.probeRadius	= 0.075f;
	debugParams.debugMode	= ddgiSubsystem.GetProbesDebugMode();

	lib::SharedPtr<DDGIDebugDrawProbesDS> debugDrawProbesDS = rdr::ResourcesManager::CreateDescriptorSetState<DDGIDebugDrawProbesDS>(RENDERER_RESOURCE_NAME("DDGIDebugDrawProbesDS"));
	debugDrawProbesDS->u_ddgiProbesDebugParams		= debugParams;
	
	const Uint32 probesNum			= ddgiSubsystem.GetProbesNum();
	const Uint32 probeVerticesNum	= 144u;
	
	const math::Vector2u renderingArea = renderView.GetRenderingResolution();

	const DepthPrepassData& depthPrepassData	= viewSpec.GetData().Get<DepthPrepassData>();

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i::Zero(), renderingArea);
	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView		= depthPrepassData.depth;
	depthRTDef.loadOperation	= rhi::ERTLoadOperation::Load;
	depthRTDef.storeOperation	= rhi::ERTStoreOperation::Store;
	depthRTDef.clearColor		= rhi::ClearColor(0.f);
	renderPassDef.SetDepthRenderTarget(depthRTDef);

	rg::RGRenderTargetDef luminanceRTDef;
	luminanceRTDef.textureView		= context.texture;
	luminanceRTDef.loadOperation		= rhi::ERTLoadOperation::Load;
	luminanceRTDef.storeOperation	= rhi::ERTStoreOperation::Store;
	luminanceRTDef.clearColor		= rhi::ClearColor(0.f, 0.f, 0.f, 1.f);
	renderPassDef.AddColorRenderTarget(luminanceRTDef);

	const rhi::EFragmentFormat colorFormat = context.texture->GetFormat();
	const rhi::EFragmentFormat depthFormat = depthPrepassData.depth->GetFormat();

	graphBuilder.RenderPass(RG_DEBUG_NAME("DDGI Draw Debug Probes"),
							renderPassDef,
							rg::BindDescriptorSets(renderView.GetRenderViewDS(),
												   std::move(debugDrawProbesDS),
												   ddgiSubsystem.GetDDGIDS()),
							[=](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), renderingArea.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), renderingArea));

								static const rdr::PipelineStateID drawDebugProbesPipeline = pipelines::CreateDDGIDrawDebugProbesPipeline(colorFormat, depthFormat);
								recorder.BindGraphicsPipeline(drawDebugProbesPipeline);

								recorder.DrawInstances(probeVerticesNum, probesNum);
							});
}

} // spt::rsc
