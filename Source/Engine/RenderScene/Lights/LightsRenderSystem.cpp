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

namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Lights Rendering Common =======================================================================

BEGIN_SHADER_STRUCT(LightsRenderingData)
	SHADER_STRUCT_FIELD(Uint32, localLightsNum)
	SHADER_STRUCT_FIELD(Uint32, localLights32Num)
	SHADER_STRUCT_FIELD(Uint32, directionalLightsNum)
	SHADER_STRUCT_FIELD(Real32, zClusterLength)
	SHADER_STRUCT_FIELD(math::Vector2u, tilesNum)
	SHADER_STRUCT_FIELD(math::Vector2f, tileSize)
	SHADER_STRUCT_FIELD(Uint32, zClustersNum)
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
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneViewData>),					u_sceneView)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneViewCullingData>),			u_sceneViewCullingData)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<LightIndirectDrawCommand>),	u_lightDraws)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),					u_lightDrawsCount)
DS_END();


DS_BEGIN(BuildLightTilesDS, rg::RGDescriptorSetState<BuildLightTilesDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<PointLightGPUData>),			u_localLights)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneViewData>),					u_sceneView)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<LightIndirectDrawCommand>),	u_lightDraws)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<LightsRenderingData>),			u_lightsData)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),					u_tilesLightsMask)
DS_END();


DS_BEGIN(ViewShadingDS, rg::RGDescriptorSetState<ViewShadingDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<LightsRenderingData>),			u_lightsData)
	DS_BINDING(BINDING_TYPE(gfx::OptionalStructuredBufferBinding<PointLightGPUData>),	u_localLights)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWStructuredBufferBinding<Uint32>),			u_tilesLightsMask)
DS_END();


BEGIN_RG_NODE_PARAMETERS_STRUCT(LightTilesIndirectDrawCommandsParameters)
	RG_BUFFER_VIEW(lightDrawCommandsBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
	RG_BUFFER_VIEW(lightDrawCommandsCountBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();


struct LightsRenderingDataPerView
{
	LightsRenderingDataPerView()
		: localLightsNum(0)
		, directionalLightsNum(0)
		, zClustersNum(0)
	{ }

	Bool HasAnyLocalLightsToRender() const
	{
		return localLightsNum > 0;
	}

	Bool HasAnyDirectionalLights() const
	{
		return directionalLightsNum > 0;
	}

	Uint32 localLightsNum;
	Uint32 directionalLightsNum;

	math::Vector2u	tilesNum;
	Uint32			zClustersNum;

	lib::SharedPtr<BuildLightZClustersDS>			buildZClustersDS;
	lib::SharedPtr<GenerateLightsDrawCommnadsDS>	generateLightsDrawCommnadsDS;
	lib::SharedPtr<BuildLightTilesDS>				buildLightTilesDS;
	lib::SharedPtr<ViewShadingDS>					viewShadingDS;

	rg::RGBufferViewHandle	lightDrawCommandsBuffer;
	rg::RGBufferViewHandle	lightDrawCommandsCountBuffer;
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// Utilities =====================================================================================

namespace utils
{

static LightsRenderingDataPerView CreateLightsRenderingData(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	constexpr Uint32 zClustersNum = 8;

	LightsRenderingDataPerView lightsRenderingData;
	
	const auto getViewSpaceZ = [viewMatrix = viewSpec.GetRenderView().GenerateViewMatrix()](const PointLightGPUData& pointLight)
	{
		math::Vector4f pointLightLocation = math::Vector4f::UnitW();
		pointLightLocation.head<3>() = pointLight.location;
		const Real32 viewSpaceZ = viewMatrix.row(3).dot(pointLightLocation);
		return viewSpaceZ;
	};

	const RenderSceneRegistry& sceneRegistry = renderScene.GetRegistry(); 
	
	const auto pointLightsView = sceneRegistry.view<const PointLightData>();
	const SizeType pointLightsNum = pointLightsView.size();

	lightsRenderingData.localLightsNum = static_cast<Uint32>(pointLightsNum);

	LightsRenderingData lightsData;

	const lib::SharedRef<ViewShadingDS> viewShadingDS = rdr::ResourcesManager::CreateDescriptorSetState<ViewShadingDS>(RENDERER_RESOURCE_NAME("ViewShadingDS"));
	lightsRenderingData.viewShadingDS = viewShadingDS;

	if (lightsRenderingData.HasAnyLocalLightsToRender())
	{
		lib::DynamicArray<PointLightGPUData> pointLights;
		lib::DynamicArray<Real32> pointLightsViewSpaceZ;
		pointLights.reserve(pointLightsNum);
		pointLightsViewSpaceZ.reserve(pointLightsNum);

		for (const auto& [entity, pointLight] : pointLightsView.each())
		{
			const PointLightGPUData gpuLightData = pointLight.GenerateGPUData();
			const Real32 lightViewSpaceZ = getViewSpaceZ(gpuLightData);

			if (lightViewSpaceZ + gpuLightData.radius > 0.f)
			{
				const auto emplaceIt = std::upper_bound(std::cbegin(pointLightsViewSpaceZ), std::cend(pointLightsViewSpaceZ), lightViewSpaceZ);
				const SizeType emplaceIdx = std::distance(std::cbegin(pointLightsViewSpaceZ), emplaceIt);

				pointLightsViewSpaceZ.emplace(emplaceIt, lightViewSpaceZ);
				pointLights.emplace(std::cbegin(pointLights) + emplaceIdx, gpuLightData);
			}
		}

		lib::DynamicArray<math::Vector2f> pointLightsZRanges;
		pointLightsZRanges.reserve(pointLights.size());

		for (SizeType idx = 0; idx < pointLights.size(); ++idx)
		{
			pointLightsZRanges.emplace_back(math::Vector2f(pointLightsViewSpaceZ[idx] - pointLights[idx].radius, pointLightsViewSpaceZ[idx] + pointLights[idx].radius));
		}

		const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingResolution();
		const math::Vector2u tilesNum = (renderingRes - math::Vector2u(1, 1)) / 8 + math::Vector2u(1, 1);
		const math::Vector2f tileSize = math::Vector2f(1.f / static_cast<Real32>(tilesNum.x()), 1.f / static_cast<Real32>(tilesNum.y()));

		lightsData.localLightsNum		= static_cast<Uint32>(pointLights.size());
		lightsData.localLights32Num		= static_cast<Uint32>((pointLights.size() - 1) / 32 + 1);
		lightsData.directionalLightsNum = 0;
		lightsData.zClusterLength		= 20.f;
		lightsData.zClustersNum			= zClustersNum;
		lightsData.tilesNum				= tilesNum;
		lightsData.tileSize				= tileSize;

		const rhi::BufferDefinition lightsBufferDefinition(pointLights.size() * sizeof(PointLightGPUData), rhi::EBufferUsage::Storage);
		const lib::SharedRef<rdr::Buffer> localLightsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("SceneLocalLights"), lightsBufferDefinition, rhi::EMemoryUsage::CPUToGPU);

		gfx::UploadDataToBuffer(localLightsBuffer, 0, reinterpret_cast<const Byte*>(pointLights.data()), pointLights.size() * sizeof(PointLightGPUData));

		const rg::RGBufferViewHandle localLightsRGBuffer = graphBuilder.AcquireExternalBufferView(localLightsBuffer->CreateFullView());

		const rhi::BufferDefinition lightZRangesBufferDefinition(pointLightsZRanges.size() * sizeof(math::Vector2f), rhi::EBufferUsage::Storage);
		const lib::SharedRef<rdr::Buffer> lightZRangesBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("SceneLightZRanges"), lightZRangesBufferDefinition, rhi::EMemoryUsage::CPUToGPU);

		gfx::UploadDataToBuffer(lightZRangesBuffer, 0, reinterpret_cast<const Byte*>(pointLightsZRanges.data()), pointLightsZRanges.size() * sizeof(math::Vector2f));

		const rhi::BufferDefinition clustersRangesBufferDefinition(zClustersNum * sizeof(math::Vector2u), rhi::EBufferUsage::Storage);
		const rg::RGBufferViewHandle clustersRanges = graphBuilder.CreateBufferView(RG_DEBUG_NAME("ClustersRanges"), clustersRangesBufferDefinition, rhi::EMemoryUsage::GPUOnly);

		const rhi::BufferDefinition lightDrawCommandsBufferDefinition(pointLights.size() * sizeof(LightIndirectDrawCommand), lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect));
		const rg::RGBufferViewHandle lightDrawCommands = graphBuilder.CreateBufferView(RG_DEBUG_NAME("LightDrawCommands"), lightDrawCommandsBufferDefinition, rhi::EMemoryUsage::GPUOnly);
		
		const rhi::BufferDefinition lightDrawCommandsCountBufferDefinition(sizeof(Uint32), lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect, rhi::EBufferUsage::TransferDst));
		const rg::RGBufferViewHandle lightDrawCommandsCount = graphBuilder.CreateBufferView(RG_DEBUG_NAME("LightDrawCommandsCount"), lightDrawCommandsCountBufferDefinition, rhi::EMemoryUsage::GPUOnly);
		graphBuilder.FillBuffer(RG_DEBUG_NAME("InitializeLightDrawCommandsCount"), lightDrawCommandsCount, 0, sizeof(Uint32), 0);

		const Uint32 tilesLightsMaskBufferSize = tilesNum.x() * tilesNum.y() * lightsData.localLights32Num * sizeof(Uint32);
		const rhi::BufferDefinition tilesLightsMaskDefinition(tilesLightsMaskBufferSize, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst));
		const rg::RGBufferViewHandle tilesLightsMask = graphBuilder.CreateBufferView(RG_DEBUG_NAME("TilesLightsMask"), tilesLightsMaskDefinition, rhi::EMemoryUsage::GPUOnly);

		graphBuilder.FillBuffer(RG_DEBUG_NAME("InitializeTilesLightsMask"), tilesLightsMask, 0, static_cast<Uint64>(tilesLightsMaskBufferSize), 0);

		const lib::SharedRef<BuildLightZClustersDS> buildZClusters = rdr::ResourcesManager::CreateDescriptorSetState<BuildLightZClustersDS>(RENDERER_RESOURCE_NAME("BuildLightZClustersDS"));
		buildZClusters->u_localLightsZRanges	= lightZRangesBuffer->CreateFullView();
		buildZClusters->u_lightsData			= lightsData;
		buildZClusters->u_clustersRanges		= clustersRanges;

		const SceneViewData sceneViewData = viewSpec.GetRenderView().GenerateViewData();

		const lib::SharedRef<GenerateLightsDrawCommnadsDS> generateLightsDrawCommnadsDS = rdr::ResourcesManager::CreateDescriptorSetState<GenerateLightsDrawCommnadsDS>(RENDERER_RESOURCE_NAME("GenerateLightsDrawCommnadsDS"));
		generateLightsDrawCommnadsDS->u_localLights		= localLightsRGBuffer;
		generateLightsDrawCommnadsDS->u_lightsData		= lightsData;
		generateLightsDrawCommnadsDS->u_sceneView		= sceneViewData;
		generateLightsDrawCommnadsDS->u_lightDraws		= lightDrawCommands;
		generateLightsDrawCommnadsDS->u_lightDrawsCount	= lightDrawCommandsCount;

		const lib::SharedRef<BuildLightTilesDS> buildLightTilesDS = rdr::ResourcesManager::CreateDescriptorSetState<BuildLightTilesDS>(RENDERER_RESOURCE_NAME("BuildLightTilesDS"));
		buildLightTilesDS->u_localLights		= localLightsRGBuffer;
		buildLightTilesDS->u_sceneView			= sceneViewData;
		buildLightTilesDS->u_lightDraws			= lightDrawCommands;
		buildLightTilesDS->u_lightsData			= lightsData;
		buildLightTilesDS->u_tilesLightsMask	= tilesLightsMask;

		viewShadingDS->u_localLights		= localLightsRGBuffer;
		viewShadingDS->u_tilesLightsMask	= tilesLightsMask;

		lightsRenderingData.buildZClustersDS				= buildZClusters;
		lightsRenderingData.generateLightsDrawCommnadsDS	= generateLightsDrawCommnadsDS;
		lightsRenderingData.buildLightTilesDS				= buildLightTilesDS;

		lightsRenderingData.lightDrawCommandsBuffer			= lightDrawCommands;
		lightsRenderingData.lightDrawCommandsCountBuffer	= lightDrawCommandsCount;
	}
	
	viewShadingDS->u_lightsData			= lightsData;
	
	lightsRenderingData.tilesNum		= lightsData.tilesNum;
	lightsRenderingData.zClustersNum	= lightsData.zClustersNum;

	return lightsRenderingData;
}

} // utils

//////////////////////////////////////////////////////////////////////////////////////////////////
// LightsRenderSystem ============================================================================

LightsRenderSystem::LightsRenderSystem()
{
	m_supportedStages = ERenderStage::ForwardOpaque;

	{
		sc::ShaderCompilationSettings compilationSettings;
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "BuildLightsZClustersCS"));
		const rdr::ShaderID buildLightsZClustersShaders = rdr::ResourcesManager::CreateShader("Sculptor/Lights/BuildLightsZClusters.hlsl", compilationSettings);
		m_buildLightsZClustersPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("BuildLightsZClusters"), buildLightsZClustersShaders);
	}

	{
		sc::ShaderCompilationSettings compilationSettings;
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "GenerateLightsDrawCommandsCS"));
		const rdr::ShaderID generateLightsDrawCommandsShader = rdr::ResourcesManager::CreateShader("Sculptor/Lights/GenerateLightsDrawCommands.hlsl", compilationSettings);
		m_generateLightsDrawCommandsPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("GenerateLightsDrawCommands"), generateLightsDrawCommandsShader);
	}
	
	{
		sc::ShaderCompilationSettings compilationSettings;
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Vertex, "BuildLightsTilesVS"));
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Fragment, "BuildLightsTilesPS"));
		const rdr::ShaderID buildLightsTilesShader = rdr::ResourcesManager::CreateShader("Sculptor/Lights/BuildLightsTiles.hlsl", compilationSettings);

		rhi::GraphicsPipelineDefinition pipelineDef;
		pipelineDef.primitiveTopology = rhi::EPrimitiveTopology::TriangleList;
		pipelineDef.rasterizationDefinition.cullMode = rhi::ECullMode::Front;
		pipelineDef.rasterizationDefinition.rasterizationType = rhi::ERasterizationType::ConservativeOverestimate;

		m_buildLightsTilesPipeline = rdr::ResourcesManager::CreateGfxPipeline(RENDERER_RESOURCE_NAME("BuildLightsTilesPipeline"), pipelineDef, buildLightsTilesShader);
	}
}

void LightsRenderSystem::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	const LightsRenderingDataPerView lightsRenderingData = utils::CreateLightsRenderingData(graphBuilder, renderScene, viewSpec);
	viewSpec.GetData().Create<LightsRenderingDataPerView>(lightsRenderingData);
		
	RenderStageEntries& forwardOpaqueEntries = viewSpec.GetRenderStageEntries(ERenderStage::ForwardOpaque);
	forwardOpaqueEntries.GetPreRenderStageDelegate().AddRawMember(this, &LightsRenderSystem::RenderPreForwardOpaquePerView);
}

void LightsRenderSystem::RenderPreForwardOpaquePerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context)
{
	SPT_PROFILER_FUNCTION();
	
	const LightsRenderingDataPerView& lightsRenderingData = viewSpec.GetData().Get<LightsRenderingDataPerView>();

	if (lightsRenderingData.HasAnyLocalLightsToRender())
	{
		const math::Vector3u dispatchZClustersGroupsNum = math::Vector3u(lightsRenderingData.zClustersNum, 1, 1);
		graphBuilder.Dispatch(RG_DEBUG_NAME("BuildLightsZClusters"), m_buildLightsZClustersPipeline, dispatchZClustersGroupsNum, rg::BindDescriptorSets(lib::Ref(lightsRenderingData.buildZClustersDS)));

		const math::Vector3u dispatchLightsGroupsNum = math::Vector3u(math::Utils::DivideCeil<Uint32>(lightsRenderingData.localLightsNum, 32), 1, 1);
		graphBuilder.Dispatch(RG_DEBUG_NAME("GenerateLightsDrawCommands"), m_generateLightsDrawCommandsPipeline, dispatchLightsGroupsNum, rg::BindDescriptorSets(lib::Ref(lightsRenderingData.generateLightsDrawCommnadsDS)));

		const math::Vector2u renderingArea = lightsRenderingData.tilesNum;

		// No render targets - we only use pixel shader to write to UAV
		const rg::RGRenderPassDefinition lightsTilesRenderPassDef(math::Vector2i::Zero(), renderingArea);

		LightTilesIndirectDrawCommandsParameters passIndirectParams;
		passIndirectParams.lightDrawCommandsBuffer = lightsRenderingData.lightDrawCommandsBuffer;
		passIndirectParams.lightDrawCommandsCountBuffer = lightsRenderingData.lightDrawCommandsCountBuffer;

		const Uint32 maxLightDrawsCount = lightsRenderingData.localLightsNum;

		graphBuilder.RenderPass(RG_DEBUG_NAME("Build Lights Tiles"),
								lightsTilesRenderPassDef,
								rg::BindDescriptorSets(lib::Ref(lightsRenderingData.buildLightTilesDS)),
								std::tie(passIndirectParams),
								[this, passIndirectParams, maxLightDrawsCount, renderingArea](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
								{
									recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), renderingArea.cast<Real32>()), 0.f, 1.f);
									recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), renderingArea));

									recorder.BindGraphicsPipeline(m_buildLightsTilesPipeline);

									const rdr::BufferView& drawsBufferView = passIndirectParams.lightDrawCommandsBuffer->GetBufferViewInstance();
									const rdr::BufferView& drawCountBufferView = passIndirectParams.lightDrawCommandsCountBuffer->GetBufferViewInstance();
									recorder.DrawIndirectCount(drawsBufferView, 0, sizeof(LightIndirectDrawCommand), drawCountBufferView, 0, maxLightDrawsCount);
								});
	}

	graphBuilder.BindDescriptorSetState(lib::Ref(lightsRenderingData.viewShadingDS));
}

} // spt::rsc
