#include "LightsRenderSystem.h"
#include "RenderScene.h"
#include "LightTypes.h"
#include "View/ViewRenderingSpec.h"
#include "View/RenderView.h"
#include "ResourcesManager.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "Transfers/UploadUtils.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "RGResources/RGResourceHandles.h"
#include "RenderGraphBuilder.h"
#include "Common/ShaderCompilationInput.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "Shadows/ShadowMapsManagerSubsystem.h"
#include "EngineFrame.h"
#include "ViewShadingInput.h"
#include "SceneRenderer/RenderStages/DirectionalLightShadowMasksRenderStage.h"
#include "Atmosphere/AtmosphereSceneSubsystem.h"
#include "Shadows/CascadedShadowMapsViewRenderSystem.h"
#include "SceneRenderer/Utils/BRDFIntegrationLUT.h"

namespace spt::rsc
{
//////////////////////////////////////////////////////////////////////////////////////////////////
// Parameters ====================================================================================

namespace params
{

RendererFloatParameter ambientLightIntensity("Ambient Light Intensity", { "Lighting" }, 0.0f, 0.f, 1.f);
RendererFloatParameter localLightsZClustersLength("Z Clusters Length", { "Lighting", "Local" }, 2.f, 1.f, 10.f);

} // params

//////////////////////////////////////////////////////////////////////////////////////////////////
// Lights Rendering Common =======================================================================

BEGIN_SHADER_STRUCT(LocalLightAreaInfo)
	SHADER_STRUCT_FIELD(Uint32, lightEntityID)
	SHADER_STRUCT_FIELD(Real32, lightAreaOnScreen)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(LightIndirectDrawCommand)
	SHADER_STRUCT_FIELD(Uint32, vertexCount)
	SHADER_STRUCT_FIELD(Uint32, instanceCount)
	SHADER_STRUCT_FIELD(Uint32, firstVertex)
	SHADER_STRUCT_FIELD(Uint32, firstInstance)
	
	SHADER_STRUCT_FIELD(Uint32, localLightIdx)
END_SHADER_STRUCT();


DS_BEGIN(BuildLightZClustersDS, rg::RGDescriptorSetState<BuildLightZClustersDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<math::Vector2f>),		u_localLightsZRanges)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<LightsRenderingData>),	u_lightsData)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<math::Vector2u>),	u_clustersRanges)
DS_END();


DS_BEGIN(GenerateLightsDrawCommnadsDS, rg::RGDescriptorSetState<GenerateLightsDrawCommnadsDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<PointLightGPUData>),			u_localLights)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<LightsRenderingData>),			u_lightsData)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneViewCullingData>),			u_sceneViewCullingData)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<LightIndirectDrawCommand>),	u_lightDraws)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),					u_lightDrawsCount)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),					u_visibleLightsReadbackBuffer)
DS_END();


DS_BEGIN(BuildLightTilesDS, rg::RGDescriptorSetState<BuildLightTilesDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<PointLightGPUData>),			u_localLights)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<LightIndirectDrawCommand>),	u_lightDraws)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<LightsRenderingData>),			u_lightsData)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),					u_tilesLightsMask)
DS_END();



BEGIN_RG_NODE_PARAMETERS_STRUCT(LightTilesIndirectDrawCommandsParameters)
	RG_BUFFER_VIEW(lightDrawCommandsBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
	RG_BUFFER_VIEW(lightDrawCommandsCountBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();


struct LightsRenderingDataPerView
{
	LightsRenderingDataPerView()
		: localLightsToRenderNum(0)
		, directionalLightsNum(0)
		, zClustersNum(0)
	{ }

	Bool HasAnyLocalLightsToRender() const
	{
		return localLightsToRenderNum > 0;
	}

	Bool HasAnyDirectionalLightsToRender() const
	{
		return directionalLightsNum > 0;
	}

	Uint32 localLightsToRenderNum;
	Uint32 directionalLightsNum;

	math::Vector2u	tilesNum;
	Uint32			zClustersNum;

	lib::MTHandle<BuildLightZClustersDS>		buildZClustersDS;
	lib::MTHandle<GenerateLightsDrawCommnadsDS>	generateLightsDrawCommnadsDS;
	lib::MTHandle<BuildLightTilesDS>			buildLightTilesDS;

	lib::MTHandle<ViewShadingInputDS> shadingInputDS;

	rg::RGBufferViewHandle	lightDrawCommandsBuffer;
	rg::RGBufferViewHandle	lightDrawCommandsCountBuffer;

	lib::SharedPtr<rdr::Buffer> visibleLightsReadbackBuffer;
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// Utilities =====================================================================================

namespace utils
{

static Uint32 CreatePointLightsData(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, rg::RGBufferViewHandle& OUT pointLightsRGBuffer, rg::RGBufferViewHandle& OUT lightZRangesRGBuffer)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	lib::DynamicArray<PointLightGPUData> pointLights;
	lib::DynamicArray<math::Vector2f> pointLightsZRanges;

	const auto getViewSpaceZ = [ viewMatrix = renderView.GetViewRenderingData().viewMatrix ](const PointLightGPUData& pointLight)
	{
		math::Vector4f pointLightLocation = math::Vector4f::UnitW();
		pointLightLocation.head<3>() = pointLight.location;
		const Real32 viewSpaceZ = viewMatrix.row(0).dot(pointLightLocation);
		return viewSpaceZ;
	};

	const RenderSceneRegistry& sceneRegistry = renderScene.GetRegistry();

	const auto pointLightsView = sceneRegistry.view<const PointLightData>();
	const SizeType pointLightsNum = pointLightsView.size();

	lib::DynamicArray<Real32> pointLightsViewSpaceZ;
	pointLights.reserve(pointLightsNum);
	pointLightsViewSpaceZ.reserve(pointLightsNum);

	for (const auto& [entity, pointLight] : pointLightsView.each())
	{
		PointLightGPUData gpuLightData = GPUDataBuilder::CreatePointLightGPUData(pointLight);
		gpuLightData.entityID = static_cast<Uint32>(entity);
		gpuLightData.shadowMapFirstFaceIdx = idxNone<Uint32>;
		if (const PointLightShadowMapComponent* shadowMapComp = sceneRegistry.try_get<const PointLightShadowMapComponent>(entity))
		{
			gpuLightData.shadowMapFirstFaceIdx = shadowMapComp->shadowMapFirstFaceIdx;
			SPT_CHECK(gpuLightData.shadowMapFirstFaceIdx != idxNone<Uint32>);
		}

		const Real32 lightViewSpaceZ = getViewSpaceZ(gpuLightData);

		if (lightViewSpaceZ + gpuLightData.radius > 0.f)
		{
			const auto emplaceIt = std::upper_bound(std::cbegin(pointLightsViewSpaceZ), std::cend(pointLightsViewSpaceZ), lightViewSpaceZ);
			const SizeType emplaceIdx = std::distance(std::cbegin(pointLightsViewSpaceZ), emplaceIt);

			pointLightsViewSpaceZ.emplace(emplaceIt, lightViewSpaceZ);
			pointLights.emplace(std::cbegin(pointLights) + emplaceIdx, gpuLightData);
		}
	}

	for (SizeType idx = 0; idx < pointLights.size(); ++idx)
	{
		pointLightsZRanges.emplace_back(math::Vector2f(pointLightsViewSpaceZ[idx] - pointLights[idx].radius, pointLightsViewSpaceZ[idx] + pointLights[idx].radius));
	}

	if (!pointLights.empty())
	{
		const rhi::BufferDefinition lightsBufferDefinition(pointLights.size() * sizeof(PointLightGPUData), rhi::EBufferUsage::Storage);
		const lib::SharedRef<rdr::Buffer> localLightsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("SceneLocalLights"), lightsBufferDefinition, rhi::EMemoryUsage::CPUToGPU);
		gfx::UploadDataToBuffer(localLightsBuffer, 0, reinterpret_cast<const Byte*>(pointLights.data()), pointLights.size() * sizeof(PointLightGPUData));
		pointLightsRGBuffer = graphBuilder.AcquireExternalBufferView(localLightsBuffer->CreateFullView());

		const rhi::BufferDefinition lightZRangesBufferDefinition(pointLightsZRanges.size() * sizeof(math::Vector2f), rhi::EBufferUsage::Storage);
		const lib::SharedRef<rdr::Buffer> lightZRangesBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("SceneLightZRanges"), lightZRangesBufferDefinition, rhi::EMemoryUsage::CPUToGPU);
		gfx::UploadDataToBuffer(lightZRangesBuffer, 0, reinterpret_cast<const Byte*>(pointLightsZRanges.data()), pointLightsZRanges.size() * sizeof(math::Vector2f));
		lightZRangesRGBuffer = graphBuilder.AcquireExternalBufferView(lightZRangesBuffer->CreateFullView());
	}

	return static_cast<Uint32>(pointLights.size());
}

static Uint32 CreateDirectionalLightsData(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const lib::MTHandle<ViewShadingInputDS>& shadingInputDS)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	const ViewDirectionalShadowMasksData* viewShadowMasks = viewSpec.GetBlackboard().Find<ViewDirectionalShadowMasksData>();

	const RenderSceneRegistry& sceneRegistry = renderScene.GetRegistry(); 

	const auto directionalLightsView = sceneRegistry.view<const DirectionalLightData, const DirectionalLightIlluminance>();
	const SizeType directionalLightsNum = sceneRegistry.view<const DirectionalLightData>().size();

	if (directionalLightsNum > 0)
	{
		lib::DynamicArray<DirectionalLightGPUData> directionalLightsData;
		directionalLightsData.reserve(directionalLightsNum);

		const auto cascadedSMSystem = renderView.FindRenderSystem<CascadedShadowMapsViewRenderSystem>();

		lib::DynamicArray<ShadowMapViewData> cascadeViewsData;

		for (const auto& [entity, directionalLight, illuminance] : directionalLightsView.each())
		{
			DirectionalLightGPUData lightGPUData = GPUDataBuilder::CreateDirectionalLightGPUData(directionalLight, illuminance);
			lightGPUData.shadowMaskIdx = idxNone<Uint32>;
			
			if (viewShadowMasks)
			{
				const auto lightShadowMask = viewShadowMasks->shadowMasks.find(entity);
				if (lightShadowMask != std::cend(viewShadowMasks->shadowMasks))
				{
					const Uint32 shadowMaskIdx = shadingInputDS->u_shadowMasks.BindTexture(lightShadowMask->second);
					lightGPUData.shadowMaskIdx = shadowMaskIdx;
				}
			}
			if (cascadedSMSystem)
			{
				const lib::DynamicArray<ShadowMap>& shadowMapViews = cascadedSMSystem->GetShadowCascadesViews(entity);

				lightGPUData.firstShadowCascadeIdx = static_cast<Uint32>(cascadeViewsData.size());
				lightGPUData.shadowCascadesNum     = static_cast<Uint32>(shadowMapViews.size());

				for (const ShadowMap& shadowMap : shadowMapViews)
				{
					shadingInputDS->u_shadowMapCascades.BindTexture(lib::Ref(shadowMap.textureView));
					cascadeViewsData.emplace_back(shadowMap.viewData);
				}
			}

			directionalLightsData.emplace_back(lightGPUData);
		}

		if (!cascadeViewsData.empty())
		{
			const Uint64 cascadeViewsBufferSize = cascadeViewsData.size() * sizeof(ShadowMapViewData);
			const rhi::BufferDefinition cascadeViewsBufferDefinition(cascadeViewsBufferSize, rhi::EBufferUsage::Storage);
			const lib::SharedRef<rdr::Buffer> shadowMapViewsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Cascade Views Data"), cascadeViewsBufferDefinition, rhi::EMemoryUsage::CPUToGPU);
			gfx::UploadDataToBuffer(shadowMapViewsBuffer, 0, reinterpret_cast<const Byte*>(cascadeViewsData.data()), cascadeViewsBufferSize);

			shadingInputDS->u_shadowMapCascadeViews = graphBuilder.AcquireExternalBufferView(shadowMapViewsBuffer->CreateFullView());
		}

		const Uint64 directionalLightsBufferSize = directionalLightsData.size() * sizeof(DirectionalLightGPUData);
		const rhi::BufferDefinition directionalLightsBufferDefinition(directionalLightsBufferSize, rhi::EBufferUsage::Storage);
		const lib::SharedRef<rdr::Buffer> directionalLightsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("View Directional Lights Buffer"), directionalLightsBufferDefinition, rhi::EMemoryUsage::CPUToGPU);
		gfx::UploadDataToBuffer(directionalLightsBuffer, 0, reinterpret_cast<const Byte*>(directionalLightsData.data()), directionalLightsBufferSize);

		shadingInputDS->u_directionalLights = directionalLightsBuffer->CreateFullView();
	}

	return static_cast<Uint32>(directionalLightsNum);
}

static LightsRenderingDataPerView CreateLightsRenderingData(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	const Uint32 zClustersNum = 32u;

	LightsRenderingDataPerView lightsRenderingDataPerView;

	LightsRenderingData lightsData;
	
	rg::RGBufferViewHandle pointLightsBuffer;
	rg::RGBufferViewHandle pointLightsZRangesBuffer;
	const Uint32 pointLightsNum = CreatePointLightsData(graphBuilder, renderScene, viewSpec, OUT pointLightsBuffer, OUT pointLightsZRangesBuffer);

	lightsRenderingDataPerView.localLightsToRenderNum = pointLightsNum;

	const lib::MTHandle<ViewShadingInputDS> shadingInputDS = graphBuilder.CreateDescriptorSet<ViewShadingInputDS>(RENDERER_RESOURCE_NAME("ViewShadingInputDS"));

	if (lightsRenderingDataPerView.HasAnyLocalLightsToRender())
	{
		const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingRes();
		const math::Vector2u tilesNum = (renderingRes - math::Vector2u(1, 1)) / 8 + math::Vector2u(1, 1);
		const math::Vector2f tileSize = math::Vector2f(1.f / static_cast<Real32>(tilesNum.x()), 1.f / static_cast<Real32>(tilesNum.y()));

		lightsData.localLightsNum			= pointLightsNum;
		lightsData.localLights32Num			= math::Utils::DivideCeil(pointLightsNum, 32u);
		lightsData.zClusterLength			= params::localLightsZClustersLength;
		lightsData.zClustersNum				= zClustersNum;
		lightsData.tilesNum					= tilesNum;
		lightsData.tileSize					= tileSize;
		lightsData.ambientLightIntensity	= params::ambientLightIntensity;

		const rhi::BufferDefinition lightDrawCommandsBufferDefinition(pointLightsNum * sizeof(LightIndirectDrawCommand), lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect));
		const rg::RGBufferViewHandle lightDrawCommands = graphBuilder.CreateBufferView(RG_DEBUG_NAME("LightDrawCommands"), lightDrawCommandsBufferDefinition, rhi::EMemoryUsage::GPUOnly);

		const rhi::BufferDefinition lightDrawCommandsCountBufferDefinition(sizeof(Uint32), lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect, rhi::EBufferUsage::TransferDst));
		const rg::RGBufferViewHandle lightDrawCommandsCount = graphBuilder.CreateBufferView(RG_DEBUG_NAME("LightDrawCommandsCount"), lightDrawCommandsCountBufferDefinition, rhi::EMemoryUsage::GPUOnly);
		graphBuilder.FillBuffer(RG_DEBUG_NAME("InitializeLightDrawCommandsCount"), lightDrawCommandsCount, 0, sizeof(Uint32), 0);

		const rhi::BufferDefinition clustersRangesBufferDefinition(zClustersNum * sizeof(math::Vector2u), lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst));
		const rg::RGBufferViewHandle clustersRanges = graphBuilder.CreateBufferView(RG_DEBUG_NAME("ClustersRanges"), clustersRangesBufferDefinition, rhi::EMemoryUsage::GPUOnly);

		const Uint64 visibleLightsReadbackBufferSize = pointLightsNum * sizeof(Uint32);
		const rhi::BufferDefinition visibleLightsReadbackBufferDef(visibleLightsReadbackBufferSize, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst));
		const lib::SharedRef<rdr::Buffer> visibleLightsReadbackBufferInstance = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("VisibleLightsReadbackBuffer"), visibleLightsReadbackBufferDef, rhi::EMemoryUsage::CPUToGPU);
		gfx::FillBuffer(visibleLightsReadbackBufferInstance, 0, visibleLightsReadbackBufferSize, idxNone<Uint32>);
		const rg::RGBufferViewHandle visibleLightsReadbackBuffer = graphBuilder.AcquireExternalBufferView(visibleLightsReadbackBufferInstance->CreateFullView());

		const Uint32 tilesLightsMaskBufferSize = tilesNum.x() * tilesNum.y() * lightsData.localLights32Num * sizeof(Uint32);
		const rhi::BufferDefinition tilesLightsMaskDefinition(tilesLightsMaskBufferSize, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst));
		const rg::RGBufferViewHandle tilesLightsMask = graphBuilder.CreateBufferView(RG_DEBUG_NAME("TilesLightsMask"), tilesLightsMaskDefinition, rhi::EMemoryUsage::GPUOnly);
		graphBuilder.FillBuffer(RG_DEBUG_NAME("InitializeTilesLightsMask"), tilesLightsMask, 0, static_cast<Uint64>(tilesLightsMaskBufferSize), 0);

		const lib::MTHandle<BuildLightZClustersDS> buildZClusters = graphBuilder.CreateDescriptorSet<BuildLightZClustersDS>(RENDERER_RESOURCE_NAME("BuildLightZClustersDS"));
		buildZClusters->u_localLightsZRanges	= pointLightsZRangesBuffer;
		buildZClusters->u_lightsData			= lightsData;
		buildZClusters->u_clustersRanges		= clustersRanges;

		const SceneViewCullingData& cullingData	= viewSpec.GetRenderView().GetCullingData();

		const lib::MTHandle<GenerateLightsDrawCommnadsDS> generateLightsDrawCommnadsDS = graphBuilder.CreateDescriptorSet<GenerateLightsDrawCommnadsDS>(RENDERER_RESOURCE_NAME("GenerateLightsDrawCommnadsDS"));
		generateLightsDrawCommnadsDS->u_localLights					= pointLightsBuffer;
		generateLightsDrawCommnadsDS->u_lightsData					= lightsData;
		generateLightsDrawCommnadsDS->u_sceneViewCullingData		= cullingData;
		generateLightsDrawCommnadsDS->u_lightDraws					= lightDrawCommands;
		generateLightsDrawCommnadsDS->u_lightDrawsCount				= lightDrawCommandsCount;
		generateLightsDrawCommnadsDS->u_visibleLightsReadbackBuffer	= visibleLightsReadbackBuffer;

		const lib::MTHandle<BuildLightTilesDS> buildLightTilesDS = graphBuilder.CreateDescriptorSet<BuildLightTilesDS>(RENDERER_RESOURCE_NAME("BuildLightTilesDS"));
		buildLightTilesDS->u_localLights		= pointLightsBuffer;
		buildLightTilesDS->u_lightDraws			= lightDrawCommands;
		buildLightTilesDS->u_lightsData			= lightsData;
		buildLightTilesDS->u_tilesLightsMask	= tilesLightsMask;

		shadingInputDS->u_localLights		= pointLightsBuffer;
		shadingInputDS->u_tilesLightsMask	= tilesLightsMask;
		shadingInputDS->u_clustersRanges	= clustersRanges;

		lightsRenderingDataPerView.buildZClustersDS				= buildZClusters;
		lightsRenderingDataPerView.generateLightsDrawCommnadsDS	= generateLightsDrawCommnadsDS;
		lightsRenderingDataPerView.buildLightTilesDS			= buildLightTilesDS;

		lightsRenderingDataPerView.lightDrawCommandsBuffer		= lightDrawCommands;
		lightsRenderingDataPerView.lightDrawCommandsCountBuffer	= lightDrawCommandsCount;
		lightsRenderingDataPerView.visibleLightsReadbackBuffer	= visibleLightsReadbackBufferInstance;
	}

	const Uint32 directionalLightsNum = CreateDirectionalLightsData(graphBuilder, renderScene, viewSpec, INOUT shadingInputDS);
	lightsData.directionalLightsNum = directionalLightsNum;

	const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();
	shadingInputDS->u_ambientOcclusionTexture = viewContext.ambientOcclusion;

	const AtmosphereSceneSubsystem& atmosphereSubsystem = renderScene.GetSceneSubsystemChecked<AtmosphereSceneSubsystem>();
	const AtmosphereContext& atmosphereContext = atmosphereSubsystem.GetAtmosphereContext();

	shadingInputDS->u_transmittanceLUT = graphBuilder.AcquireExternalTextureView(atmosphereContext.transmittanceLUT);
	shadingInputDS->u_atmosphereParams = atmosphereContext.atmosphereParamsBuffer->CreateFullView();
	
	shadingInputDS->u_lightsData = lightsData;
	
	lightsRenderingDataPerView.shadingInputDS = shadingInputDS;

	lightsRenderingDataPerView.tilesNum		= lightsData.tilesNum;
	lightsRenderingDataPerView.zClustersNum	= lightsData.zClustersNum;

	return lightsRenderingDataPerView;
}

} // utils

//////////////////////////////////////////////////////////////////////////////////////////////////
// LightsRenderSystem ============================================================================

LightsRenderSystem::LightsRenderSystem()
{
	m_supportedStages = lib::Flags(ERenderStage::ForwardOpaque, ERenderStage::DeferredShading);
	
	m_globalLightsDS = rdr::ResourcesManager::CreateDescriptorSetState<GlobalLightsDS>(RENDERER_RESOURCE_NAME("Global Lights DS"));

	{
		const rdr::ShaderID buildLightsZClustersShaders = rdr::ResourcesManager::CreateShader("Sculptor/Lights/BuildLightsZClusters.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "BuildLightsZClustersCS"));
		m_buildLightsZClustersPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("BuildLightsZClusters"), buildLightsZClustersShaders);
	}

	{
		const rdr::ShaderID generateLightsDrawCommandsShader = rdr::ResourcesManager::CreateShader("Sculptor/Lights/GenerateLightsDrawCommands.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "GenerateLightsDrawCommandsCS"));
		m_generateLightsDrawCommandsPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("GenerateLightsDrawCommands"), generateLightsDrawCommandsShader);
	}
	
	{
		rdr::GraphicsPipelineShaders shaders;
		shaders.vertexShader = rdr::ResourcesManager::CreateShader("Sculptor/Lights/BuildLightsTiles.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Vertex, "BuildLightsTilesVS"));
		shaders.fragmentShader = rdr::ResourcesManager::CreateShader("Sculptor/Lights/BuildLightsTiles.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Fragment, "BuildLightsTilesPS"));

		rhi::GraphicsPipelineDefinition pipelineDef;
		pipelineDef.primitiveTopology = rhi::EPrimitiveTopology::TriangleList;
		pipelineDef.rasterizationDefinition.cullMode = rhi::ECullMode::Front;
		pipelineDef.rasterizationDefinition.rasterizationType = rhi::ERasterizationType::ConservativeOverestimate;

		m_buildLightsTilesPipeline = rdr::ResourcesManager::CreateGfxPipeline(RENDERER_RESOURCE_NAME("BuildLightsTilesPipeline"), shaders, pipelineDef);
	}
}

void LightsRenderSystem::CollectRenderViews(const RenderScene& renderScene, const RenderView& mainRenderView, INOUT RenderViewsCollector& viewsCollector)
{
	SPT_PROFILER_FUNCTION();

	Super::CollectRenderViews(renderScene, mainRenderView, viewsCollector);

	if (const lib::SharedPtr<ShadowMapsManagerSubsystem> shadowMapsManager = renderScene.GetSceneSubsystem<ShadowMapsManagerSubsystem>())
	{
		const lib::DynamicArray<RenderView*> shadowMapViewsToRender = shadowMapsManager->GetShadowMapViewsToUpdate();
		for (RenderView* renderView : shadowMapViewsToRender)
		{
			SPT_CHECK(!!renderView);
			viewsCollector.AddRenderView(*renderView);
		}
	}
}

void LightsRenderSystem::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs)
{
	Super::RenderPerFrame(graphBuilder, renderScene, viewSpecs);

	// Cache shadow maps for each views for this frame
	CacheShadowMapsDS(graphBuilder, renderScene);

	CacheGlobalLightsDS(graphBuilder, renderScene);

	for (ViewRenderingSpec* viewSpec : viewSpecs)
	{
		SPT_CHECK(!!viewSpec);
		RenderPerView(graphBuilder, renderScene, *viewSpec);
	}
}

const lib::MTHandle<GlobalLightsDS>& LightsRenderSystem::GetGlobalLightsDS() const
{
	return m_globalLightsDS;
}

void LightsRenderSystem::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	// if this view supports directional shadow masks, we need to render them before creating shading input to properly cache shadow masks
	if (viewSpec.SupportsStage(ERenderStage::ForwardOpaque))
	{
		RenderStageEntries& stageEntries = viewSpec.GetRenderStageEntries(ERenderStage::ForwardOpaque);
		stageEntries.GetPreRenderStage().AddRawMember(this, &LightsRenderSystem::BuildLightsTiles);
	}
	else if (viewSpec.SupportsStage(ERenderStage::DeferredShading))
	{
		RenderStageEntries& stageEntries = viewSpec.GetRenderStageEntries(ERenderStage::DeferredShading);
		stageEntries.GetPreRenderStage().AddRawMember(this, &LightsRenderSystem::BuildLightsTiles);
	}
}

void LightsRenderSystem::BuildLightsTiles(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context)
{
	SPT_PROFILER_FUNCTION();

	ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();
	
	const LightsRenderingDataPerView lightsRenderingData = utils::CreateLightsRenderingData(graphBuilder, renderScene, viewSpec);

	if (lightsRenderingData.HasAnyLocalLightsToRender())
	{
		// Schedule visible lights read back
		const lib::SharedPtr<ShadowMapsManagerSubsystem> shadowMapsSystem = renderScene.GetSceneSubsystem<ShadowMapsManagerSubsystem>();
		if (shadowMapsSystem->IsMainView(viewSpec.GetRenderView()))
		{
			const lib::WeakPtr<ShadowMapsManagerSubsystem> weakShadowMapsManager = shadowMapsSystem;
			js::Launch("Update Visible Local Lights",
					   [weakShadowMapsManager, visibleLightsBuffer = lightsRenderingData.visibleLightsReadbackBuffer]
					   {
						   const lib::SharedPtr<ShadowMapsManagerSubsystem> shadowMapsManager = weakShadowMapsManager.lock();
						   if (shadowMapsManager)
						   {
							   shadowMapsManager->UpdateVisibleLocalLights(visibleLightsBuffer);
						   }
					   },
					   js::Prerequisites(graphBuilder.GetGPUFinishedEvent()));
		}

		RenderView& renderView = viewSpec.GetRenderView();

		const math::Vector3u dispatchZClustersGroupsNum = math::Vector3u(lightsRenderingData.zClustersNum, 1, 1);
		graphBuilder.Dispatch(RG_DEBUG_NAME("BuildLightsZClusters"),
							  m_buildLightsZClustersPipeline,
							  dispatchZClustersGroupsNum,
							  rg::BindDescriptorSets(lightsRenderingData.buildZClustersDS));

		const math::Vector3u dispatchLightsGroupsNum = math::Vector3u(math::Utils::DivideCeil<Uint32>(lightsRenderingData.localLightsToRenderNum, 32), 1, 1);
		graphBuilder.Dispatch(RG_DEBUG_NAME("GenerateLightsDrawCommands"),
							  m_generateLightsDrawCommandsPipeline,
							  dispatchLightsGroupsNum,
							  rg::BindDescriptorSets(lightsRenderingData.generateLightsDrawCommnadsDS, viewContext.depthCullingDS, renderView.GetRenderViewDS()));

		const math::Vector2u renderingArea = lightsRenderingData.tilesNum;

		// No render targets - we only use pixel shader to write to UAV
		const rg::RGRenderPassDefinition lightsTilesRenderPassDef(math::Vector2i::Zero(), renderingArea);

		LightTilesIndirectDrawCommandsParameters passIndirectParams;
		passIndirectParams.lightDrawCommandsBuffer = lightsRenderingData.lightDrawCommandsBuffer;
		passIndirectParams.lightDrawCommandsCountBuffer = lightsRenderingData.lightDrawCommandsCountBuffer;

		const Uint32 maxLightDrawsCount = lightsRenderingData.localLightsToRenderNum;

		graphBuilder.RenderPass(RG_DEBUG_NAME("Build Lights Tiles"),
								lightsTilesRenderPassDef,
								rg::BindDescriptorSets(lightsRenderingData.buildLightTilesDS, renderView.GetRenderViewDS()),
								std::tie(passIndirectParams),
								[this, passIndirectParams, maxLightDrawsCount, renderingArea](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
								{
									recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), renderingArea.cast<Real32>()), 0.f, 1.f);
									recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), renderingArea));

									recorder.BindGraphicsPipeline(m_buildLightsTilesPipeline);

									const rdr::BufferView& drawsBufferView = passIndirectParams.lightDrawCommandsBuffer->GetResource();
									const rdr::BufferView& drawCountBufferView = passIndirectParams.lightDrawCommandsCountBuffer->GetResource();
									recorder.DrawIndirectCount(drawsBufferView, 0, sizeof(LightIndirectDrawCommand), drawCountBufferView, 0, maxLightDrawsCount);
								});
	}

	viewContext.shadingInputDS = lightsRenderingData.shadingInputDS;
	viewContext.shadowMapsDS   = m_shadowMapsDS;

	ViewSpecShadingParameters shadingParams;
	shadingParams.shadingInputDS	= lightsRenderingData.shadingInputDS;
	shadingParams.shadowMapsDS		= m_shadowMapsDS;
	viewSpec.GetBlackboard().Create<ViewSpecShadingParameters>(shadingParams);
}

void LightsRenderSystem::CacheGlobalLightsDS(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene)
{
	const RenderSceneRegistry& sceneRegistry = renderScene.GetRegistry();

	const auto pointLightsView = sceneRegistry.view<const PointLightData>();
	const SizeType pointLightsNum = pointLightsView.size();

	lib::SharedPtr<rdr::Buffer> pointLightsBuffer;
	if (pointLightsNum > 0)
	{
		const Uint64 pointLightsDataSize = pointLightsNum * sizeof(PointLightGPUData);
		const rhi::BufferDefinition pointLightsDataBufferDef(pointLightsDataSize, rhi::EBufferUsage::Storage);
		pointLightsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Global Point Lights Buffer"), pointLightsDataBufferDef, rhi::EMemoryUsage::CPUToGPU);
		rhi::RHIMappedBuffer<PointLightGPUData> pointLightsData(pointLightsBuffer->GetRHI());

		SizeType pointLightIdx = 0;

		for (const auto& [entity, pointLight] : pointLightsView.each())
		{
			PointLightGPUData& gpuLightData = pointLightsData[pointLightIdx++];
			gpuLightData = GPUDataBuilder::CreatePointLightGPUData(pointLight);
			gpuLightData.entityID = static_cast<Uint32>(entity);
			gpuLightData.shadowMapFirstFaceIdx = idxNone<Uint32>;

			if (const PointLightShadowMapComponent* shadowMapComp = sceneRegistry.try_get<const PointLightShadowMapComponent>(entity))
			{
				gpuLightData.shadowMapFirstFaceIdx = shadowMapComp->shadowMapFirstFaceIdx;
				SPT_CHECK(gpuLightData.shadowMapFirstFaceIdx != idxNone<Uint32>);
			}
		}
	}

	const auto directionalLightsView = sceneRegistry.view<const DirectionalLightData, const DirectionalLightIlluminance>();
	const SizeType directionalLightsNum = sceneRegistry.view<const DirectionalLightData>().size();

	lib::SharedPtr<rdr::Buffer> directionalLightsBuffer;
	if (directionalLightsNum > 0)
	{
		const Uint64 directionalLightsDataSize = directionalLightsNum * sizeof(DirectionalLightGPUData);
		const rhi::BufferDefinition directionalLightsDataBufferDef(directionalLightsDataSize, rhi::EBufferUsage::Storage);
		directionalLightsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Global Directional Lights Buffer"), directionalLightsDataBufferDef, rhi::EMemoryUsage::CPUToGPU);
		rhi::RHIMappedBuffer<DirectionalLightGPUData> directionalLightsData(directionalLightsBuffer->GetRHI());

		SizeType directionalLightIdx = 0;

		for (const auto& [entity, directionalLight, illuminance] : directionalLightsView.each())
		{
			SPT_CHECK(directionalLightIdx < directionalLightsData.GetElementsNum());

			DirectionalLightGPUData& lightGPUData = directionalLightsData[directionalLightIdx++];
			lightGPUData = GPUDataBuilder::CreateDirectionalLightGPUData(directionalLight, illuminance);
			lightGPUData.shadowMaskIdx = idxNone<Uint32>;
		}
	}

	GlobalLightsParams lightsParams;
	lightsParams.pointLightsNum       = static_cast<Uint32>(pointLightsNum);
	lightsParams.directionalLightsNum = static_cast<Uint32>(directionalLightsNum);

	m_globalLightsDS->u_lightsParams       = lightsParams;
	m_globalLightsDS->u_pointLights        = pointLightsBuffer->CreateFullView();
	m_globalLightsDS->u_directionalLights  = directionalLightsBuffer->CreateFullView();
	m_globalLightsDS->u_brdfIntegrationLUT = BRDFIntegrationLUT::Get().GetLUT(graphBuilder);
}

void LightsRenderSystem::CacheShadowMapsDS(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene)
{
	lib::SharedPtr<ShadowMapsManagerSubsystem> shadowMapsManager = renderScene.GetSceneSubsystem<ShadowMapsManagerSubsystem>();
	if (shadowMapsManager && shadowMapsManager->CanRenderShadows())
	{
		m_shadowMapsDS = shadowMapsManager->GetShadowMapsDS();
	}
	else
	{
		m_shadowMapsDS = graphBuilder.CreateDescriptorSet<ShadowMapsDS>(RENDERER_RESOURCE_NAME("ShadowMapsDS"));
	}
}

} // spt::rsc
