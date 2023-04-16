#include "DDGIRenderSystem.h"
#include "RenderScene.h"
#include "DDGISceneSubsystem.h"
#include "RenderGraphBuilder.h"
#include "ResourcesManager.h"
#include "Utils/BufferUtils.h"
#include "RayTracing/RayTracingRenderSceneSubsystem.h"
#include "Common/ShaderCompilationInput.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/AccelerationStructureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/ConstantBufferRefBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "SceneRenderer/SceneRendererTypes.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"

namespace spt::rsc
{

namespace parameters
{

RendererBoolParameter ddgiEnabled("Enable DDGI", {"DDGI"}, true);

} // parameters

//////////////////////////////////////////////////////////////////////////////////////////////////
// Descriptor Sets ===============================================================================

BEGIN_SHADER_STRUCT(DDGIBlendParams)
	SHADER_STRUCT_FIELD(math::Vector2f, traceRaysResultTexturePixelSize)
END_SHADER_STRUCT();


DS_BEGIN(DDGITraceRaysDS, rg::RGDescriptorSetState<DDGITraceRaysDS>)
	DS_BINDING(BINDING_TYPE(gfx::AccelerationStructureBinding),							u_sceneAccelerationStructure)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<RTInstanceData>),				u_rtInstances)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),					u_traceRaysResultTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<DDGIUpdateProbesGPUParams>),	u_updateProbesParams)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<DDGIGPUParams>),				u_ddgiParams)
DS_END();


DS_BEGIN(DDGIBlendProbesDataDS, rg::RGDescriptorSetState<DDGIBlendProbesDataDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),								u_traceRaysResultTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_traceRaysResultSampler)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<DDGIUpdateProbesGPUParams>),				u_updateProbesParams)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<DDGIGPUParams>),							u_ddgiParams)
DS_END();


DS_BEGIN(DDGIUpdateProbesIrradianceDS, rg::RGDescriptorSetState<DDGIUpdateProbesIrradianceDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>), u_probesIrradianceTexture)
DS_END();


DS_BEGIN(DDGIUpdateProbesHitDistanceDS, rg::RGDescriptorSetState<DDGIUpdateProbesHitDistanceDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector2f>), u_probesHitDistanceTexture)
DS_END();


DS_BEGIN(DDGIDebugDrawProbesDS, rg::RGDescriptorSetState<DDGIDebugDrawProbesDS>)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<DDGIGPUParams>),					u_ddgiParams)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),								u_probesIrradianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),								u_probesHitDistanceTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),	u_probesDataSampler)
DS_END();

//////////////////////////////////////////////////////////////////////////////////////////////////
// Pipelines =====================================================================================

namespace pipelines
{

static rdr::PipelineStateID CreateDDGITraceRaysPipeline()
{

	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.DisableGeneratingDebugSource();
	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::RTGeneration, "DDGIProbeRaysRTG"));
	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::RTMiss, "DDGIProbeRaysRTM"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/DDGI/DDGITraceRays.hlsl", compilationSettings);

	rhi::RayTracingPipelineDefinition pipelineDefinition;
	pipelineDefinition.maxRayRecursionDepth = 1;
	return rdr::ResourcesManager::CreateRayTracingPipeline(RENDERER_RESOURCE_NAME("DDGI Trace Rays Pipeline"), shader, pipelineDefinition);
}

static rdr::PipelineStateID CreateDDGIBlendProbesIrradiancePipeline(math::Vector2u groupSize, Uint32 raysNumPerProbe)
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("DDGI_BLEND_TYPE", "1")); // Blend Irradiance
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("GROUP_SIZE_X", std::to_string(groupSize.x())));
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("GROUP_SIZE_Y", std::to_string(groupSize.y())));
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("RAYS_NUM_PER_PROBE", std::to_string(raysNumPerProbe)));

	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "DDGIBlendProbesDataCS"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/DDGI/DDGIBlendProbesData.hlsl", compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("DDGI Blend Probes Irradiance Pipeline"), shader);
}

static rdr::PipelineStateID CreateDDGIBlendProbesHitDistancePipeline(math::Vector2u groupSize, Uint32 raysNumPerProbe)
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("DDGI_BLEND_TYPE", "2")); // Blend hit distance
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("GROUP_SIZE_X", std::to_string(groupSize.x())));
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("GROUP_SIZE_Y", std::to_string(groupSize.y())));
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("RAYS_NUM_PER_PROBE", std::to_string(raysNumPerProbe)));

	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "DDGIBlendProbesDataCS"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/DDGI/DDGIBlendProbesData.hlsl", compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("DDGI Blend Probes Hit Distance Pipeline"), shader);
}

static rdr::PipelineStateID CreateDDGIDrawDebugProbesPipeline(rhi::EFragmentFormat colorRTFormat, rhi::EFragmentFormat depthFormat)
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Vertex, "DDGIDebugProbesVS"));
	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Fragment, "DDGIDebugProbesPS"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/DDGI/DDGIDebugProbes.hlsl", compilationSettings);

	rhi::GraphicsPipelineDefinition pipelineDef;
	pipelineDef.primitiveTopology = rhi::EPrimitiveTopology::TriangleList;
	pipelineDef.renderTargetsDefinition.depthRTDefinition.format = depthFormat;
	pipelineDef.renderTargetsDefinition.depthRTDefinition.depthCompareOp = spt::rhi::ECompareOp::GreaterOrEqual;
	pipelineDef.renderTargetsDefinition.colorRTsDefinition.emplace_back(rhi::ColorRenderTargetDefinition(colorRTFormat));

	return rdr::ResourcesManager::CreateGfxPipeline(RENDERER_RESOURCE_NAME("DDGI Debug Probes Pipeline"), pipelineDef, shader);
}

} // pipelines

//////////////////////////////////////////////////////////////////////////////////////////////////
// DDGIUpdateParameters ==========================================================================

DDGIUpdateParameters::DDGIUpdateParameters(const DDGIUpdateProbesGPUParams& gpuUpdateParams, const DDGISceneSubsystem& ddgiSubsystem)
	: probesIrradianceTextureView(ddgiSubsystem.GetProbesIrradianceTexture())
	, probesHitDistanceTextureView(ddgiSubsystem.GetProbesHitDistanceTexture())
	, updateProbesParamsBuffer(rdr::utils::CreateConstantBufferView<DDGIUpdateProbesGPUParams>(RENDERER_RESOURCE_NAME("DDGIUpdateProbesGPUParams"), gpuUpdateParams))
	, ddgiParamsBuffer(rdr::utils::CreateConstantBufferView<DDGIGPUParams>(RENDERER_RESOURCE_NAME("DDGIGPUParams"), ddgiSubsystem.GetDDGIParams()))
	, probesNumToUpdate(gpuUpdateParams.probesNumToUpdate)
	, raysNumPerProbe(gpuUpdateParams.raysNumPerProbe)
	, probeIrradianceDataWithBorderRes(ddgiSubsystem.GetDDGIParams().probeIrradianceDataWithBorderRes)
	, probeHitDistanceDataWithBorderRes(ddgiSubsystem.GetDDGIParams().probeHitDistanceDataWithBorderRes)
{ }

//////////////////////////////////////////////////////////////////////////////////////////////////
// DDGIRenderSystem ==============================================================================

DDGIRenderSystem::DDGIRenderSystem()
{
	m_supportedStages = spt::rsc::ERenderStage::ForwardOpaque;
}

void DDGIRenderSystem::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene)
{
	SPT_PROFILER_FUNCTION();

	Super::RenderPerFrame(graphBuilder, renderScene);

	if (parameters::ddgiEnabled)
	{
		DDGISceneSubsystem& ddgiSubsystem = renderScene.GetSceneSubsystemChecked<DDGISceneSubsystem>();

		const DDGIUpdateParameters updateParams(ddgiSubsystem.CreateUpdateProbesParams(), ddgiSubsystem);

		UpdateProbes(graphBuilder, renderScene, updateParams);
	}
}

void DDGIRenderSystem::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	Super::RenderPerView(graphBuilder, renderScene, viewSpec);

	const DDGISceneSubsystem& ddgiSubsystem = renderScene.GetSceneSubsystemChecked<DDGISceneSubsystem>();
	
	const EDDDGIProbesDebugType probesDebug = ddgiSubsystem.GetProbesDebugType();
	if (probesDebug != EDDDGIProbesDebugType::None)
	{
		RenderStageEntries& foEntries = viewSpec.GetRenderStageEntries(ERenderStage::ForwardOpaque);
		foEntries.GetPostRenderStage().AddRawMember(this, &DDGIRenderSystem::RenderDebugProbes, probesDebug);
	}
}

void DDGIRenderSystem::UpdateProbes(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const DDGIUpdateParameters& updateParams) const
{
	SPT_PROFILER_FUNCTION();

	const rg::RGTextureViewHandle probesTraceResultTexture = TraceRays(graphBuilder, renderScene, updateParams);

	const lib::SharedRef<DDGIBlendProbesDataDS> updateProbesDS = rdr::ResourcesManager::CreateDescriptorSetState<DDGIBlendProbesDataDS>(RENDERER_RESOURCE_NAME("DDGIBlendProbesDataDS"));
	updateProbesDS->u_traceRaysResultTexture	= probesTraceResultTexture;
	updateProbesDS->u_updateProbesParams		= updateParams.updateProbesParamsBuffer;
	updateProbesDS->u_ddgiParams				= updateParams.ddgiParamsBuffer;

	lib::SharedPtr<DDGIUpdateProbesIrradianceDS> updateProbesIrradianceDS = rdr::ResourcesManager::CreateDescriptorSetState<DDGIUpdateProbesIrradianceDS>(RENDERER_RESOURCE_NAME("DDGIUpdateProbesIrradianceDS"));
	updateProbesIrradianceDS->u_probesIrradianceTexture = updateParams.probesIrradianceTextureView;

	static const rdr::PipelineStateID updateProbesIrradiancePipelineID = pipelines::CreateDDGIBlendProbesIrradiancePipeline(updateParams.probeIrradianceDataWithBorderRes, updateParams.raysNumPerProbe);
	
	graphBuilder.Dispatch(RG_DEBUG_NAME("DDGI Update Probes Irradiance"),
						  updateProbesIrradiancePipelineID,
						  math::Vector3u(updateParams.probesNumToUpdate, 1u, 1u),
						  rg::BindDescriptorSets(updateProbesDS,
												 std::move(updateProbesIrradianceDS)));

	lib::SharedPtr<DDGIUpdateProbesHitDistanceDS> updateProbesHitDistanceDS = rdr::ResourcesManager::CreateDescriptorSetState<DDGIUpdateProbesHitDistanceDS>(RENDERER_RESOURCE_NAME("DDGIUpdateProbesHitDistanceDS"));
	updateProbesHitDistanceDS->u_probesHitDistanceTexture = updateParams.probesHitDistanceTextureView;
	
	static const rdr::PipelineStateID updateProbesDistancesPipelineID = pipelines::CreateDDGIBlendProbesHitDistancePipeline(updateParams.probeHitDistanceDataWithBorderRes, updateParams.raysNumPerProbe);;

	graphBuilder.Dispatch(RG_DEBUG_NAME("DDGI Update Probes Hit Distance"),
						  updateProbesDistancesPipelineID,
						  math::Vector3u(updateParams.probesNumToUpdate, 1u, 1u),
						  rg::BindDescriptorSets(updateProbesDS,
												 std::move(updateProbesHitDistanceDS)));
}

rg::RGTextureViewHandle DDGIRenderSystem::TraceRays(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const DDGIUpdateParameters& updateParams) const
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

	const lib::SharedPtr<DDGITraceRaysDS> traceRaysDS = rdr::ResourcesManager::CreateDescriptorSetState<DDGITraceRaysDS>(RENDERER_RESOURCE_NAME("DDGITraceRaysDS"));
	traceRaysDS->u_sceneAccelerationStructure	= lib::Ref(rayTracingSubsystem.GetSceneTLAS());
	traceRaysDS->u_rtInstances					= rayTracingSubsystem.GetRTInstancesDataBuffer()->CreateFullView();
	traceRaysDS->u_traceRaysResultTexture		= probesTraceResultTexture;
	traceRaysDS->u_updateProbesParams			= updateParams.updateProbesParamsBuffer;
	traceRaysDS->u_ddgiParams					= updateParams.ddgiParamsBuffer;

	static const rdr::PipelineStateID traceRaysPipelineID = pipelines::CreateDDGITraceRaysPipeline();

	graphBuilder.TraceRays(RG_DEBUG_NAME("DDGI Trace Rays"),
						   traceRaysPipelineID,
						   math::Vector3u(probesToUpdateNum, updateParams.raysNumPerProbe, 1u),
						   rg::BindDescriptorSets(traceRaysDS));

	return probesTraceResultTexture;
}

void DDGIRenderSystem::RenderDebugProbes(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context, EDDDGIProbesDebugType debugType) const
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const DDGISceneSubsystem& ddgiSubsystem = renderScene.GetSceneSubsystemChecked<DDGISceneSubsystem>();

	lib::SharedPtr<DDGIDebugDrawProbesDS> debugDrawProbesDS = rdr::ResourcesManager::CreateDescriptorSetState<DDGIDebugDrawProbesDS>(RENDERER_RESOURCE_NAME("DDGIDebugDrawProbesDS"));
	debugDrawProbesDS->u_ddgiParams					= ddgiSubsystem.GetDDGIParams();
	debugDrawProbesDS->u_probesIrradianceTexture	= ddgiSubsystem.GetProbesIrradianceTexture();
	debugDrawProbesDS->u_probesHitDistanceTexture	= ddgiSubsystem.GetProbesHitDistanceTexture();
	
	const Uint32 probesNum			= ddgiSubsystem.GetProbesNum();
	const Uint32 probeVerticesNum	= 144u;
	
	const math::Vector2u renderingArea = renderView.GetRenderingResolution();

	const DepthPrepassData& depthPrepassData	= viewSpec.GetData().Get<DepthPrepassData>();
	const ShadingData& shadingPassData			= viewSpec.GetData().Get<ShadingData>();

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i::Zero(), renderingArea);
	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView		= depthPrepassData.depth;
	depthRTDef.loadOperation	= rhi::ERTLoadOperation::Load;
	depthRTDef.storeOperation	= rhi::ERTStoreOperation::Store;
	depthRTDef.clearColor		= rhi::ClearColor(0.f);
	renderPassDef.SetDepthRenderTarget(depthRTDef);

	rg::RGRenderTargetDef radianceRTDef;
	radianceRTDef.textureView		= shadingPassData.radiance;
	radianceRTDef.loadOperation		= rhi::ERTLoadOperation::Load;
	radianceRTDef.storeOperation	= rhi::ERTStoreOperation::Store;
	radianceRTDef.clearColor		= rhi::ClearColor(0.f, 0.f, 0.f, 1.f);
	renderPassDef.AddColorRenderTarget(radianceRTDef);

	const rhi::EFragmentFormat colorFormat = shadingPassData.radiance->GetFormat();
	const rhi::EFragmentFormat depthFormat = depthPrepassData.depth->GetFormat();

	graphBuilder.RenderPass(RG_DEBUG_NAME("DDGI Draw Debug Probes"),
							renderPassDef,
							rg::BindDescriptorSets(renderView.GetRenderViewDS(),
												   std::move(debugDrawProbesDS)),
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
