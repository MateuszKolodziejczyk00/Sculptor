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

BEGIN_SHADER_STRUCT(SceneLightsInfo)
	SHADER_STRUCT_FIELD(Uint32, localLightsNum)
	SHADER_STRUCT_FIELD(Uint32, localLights32Num)
	SHADER_STRUCT_FIELD(Uint32, directionalLightsNum)
	SHADER_STRUCT_FIELD(Real32, zClusterLength)
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
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneLightsInfo>),		u_sceneLightsInfo)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<math::Vector2u>),	u_clustersRanges)
DS_END();


DS_BEGIN(GenerateLightsDrawCommnadsDS, rg::RGDescriptorSetState<GenerateLightsDrawCommnadsDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<PointLightGPUData>),			u_localLights)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneLightsInfo>),				u_sceneLightsInfo)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneViewData>),					u_sceneView)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneViewCullingData>),			u_sceneViewCullingData)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<LightIndirectDrawCommand>),	u_lightDraws)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),					u_lightDrawsCount)
DS_END();


struct LightsRenderingDataPerView
{
	lib::SharedPtr<BuildLightZClustersDS>			buildZClustersDS;
	lib::SharedPtr<GenerateLightsDrawCommnadsDS>	generateLightsDrawCommnadsDS;
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// Utilities =====================================================================================

namespace utils
{

static LightsRenderingDataPerView CreateLightsRenderingData(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	constexpr Uint32 zClustersNum = 8;

	LightsRenderingDataPerView lightsData;
	
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
	if (pointLightsNum > 0)
	{
		lib::DynamicArray<PointLightGPUData> pointLights;
		lib::DynamicArray<Real32> pointLightsViewSpaceZ;
		pointLights.reserve(pointLightsNum);
		pointLightsViewSpaceZ.reserve(pointLightsNum);

		for (const auto& [entity, pointLight] : pointLightsView.each())
		{
			const PointLightGPUData gpuLightData = pointLight.GenerateGPUData();
			const Real32 lightViewSpaceZ = getViewSpaceZ(pointLights.back());

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

		const rhi::BufferDefinition lightsBufferDefinition(pointLights.size() * sizeof(PointLightGPUData), rhi::EBufferUsage::Storage);
		const lib::SharedRef<rdr::Buffer> localLightsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("SceneLocalLights"), lightsBufferDefinition, rhi::EMemoryUsage::CPUToGPU);

		gfx::UploadDataToBuffer(localLightsBuffer, 0, reinterpret_cast<const Byte*>(pointLights.data()), pointLights.size() * sizeof(PointLightGPUData));

		const rhi::BufferDefinition lightZRangesBufferDefinition(pointLightsZRanges.size() * sizeof(math::Vector2f), rhi::EBufferUsage::Storage);
		const lib::SharedRef<rdr::Buffer> lightZRangesBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("SceneLightZRanges"), lightZRangesBufferDefinition, rhi::EMemoryUsage::CPUToGPU);

		gfx::UploadDataToBuffer(lightZRangesBuffer, 0, reinterpret_cast<const Byte*>(pointLightsZRanges.data()), pointLightsZRanges.size() * sizeof(math::Vector2f));

		const rhi::BufferDefinition clustersRangesBufferDefinition(zClustersNum * sizeof(math::Vector2u), rhi::EBufferUsage::Storage);
		const rg::RGBufferViewHandle clustersRanges = graphBuilder.CreateBufferView(RG_DEBUG_NAME("ClustersRanges"), clustersRangesBufferDefinition, rhi::EMemoryUsage::GPUOnly);

		const rhi::BufferDefinition lightDrawCommandsBufferDefinition(pointLights.size() * sizeof(LightIndirectDrawCommand), lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect));
		const rg::RGBufferViewHandle lightDrawCommands = graphBuilder.CreateBufferView(RG_DEBUG_NAME("LightDrawCommands"), lightDrawCommandsBufferDefinition, rhi::EMemoryUsage::GPUOnly);
		
		const rhi::BufferDefinition lightDrawCommandsCountBufferDefinition(sizeof(Uint32), lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect));
		const rg::RGBufferViewHandle lightDrawCommandsCount = graphBuilder.CreateBufferView(RG_DEBUG_NAME("LightDrawCommandsCount"), lightDrawCommandsCountBufferDefinition, rhi::EMemoryUsage::GPUOnly);
		graphBuilder.FillBuffer(RG_DEBUG_NAME("InitializeLightDrawCommandsCount"), lightDrawCommandsCount, 0, sizeof(Uint32), 0);

		SceneLightsInfo lightsInfo;
		lightsInfo.localLightsNum		= static_cast<Uint32>(pointLights.size());
		lightsInfo.localLights32Num		= static_cast<Uint32>((pointLights.size() - 1) / 32 + 1);
		lightsInfo.directionalLightsNum = 0;
		lightsInfo.zClusterLength		= 20.f;
		lightsInfo.zClustersNum			= zClustersNum;

		const lib::SharedRef<BuildLightZClustersDS> buildZClusters = rdr::ResourcesManager::CreateDescriptorSetState<BuildLightZClustersDS>(RENDERER_RESOURCE_NAME("BuildLightZClustersDS"));
		buildZClusters->u_localLightsZRanges	= lightZRangesBuffer->CreateFullView();
		buildZClusters->u_sceneLightsInfo		= lightsInfo;
		buildZClusters->u_clustersRanges		= clustersRanges;

		const lib::SharedRef<GenerateLightsDrawCommnadsDS> generateLightsDrawCommnadsDS = rdr::ResourcesManager::CreateDescriptorSetState<GenerateLightsDrawCommnadsDS>(RENDERER_RESOURCE_NAME("GenerateLightsDrawCommnadsDS"));
		generateLightsDrawCommnadsDS->u_localLights		= localLightsBuffer->CreateFullView();
		generateLightsDrawCommnadsDS->u_sceneLightsInfo	= lightsInfo;
		generateLightsDrawCommnadsDS->u_sceneView		= viewSpec.GetRenderView().GenerateViewData();
		generateLightsDrawCommnadsDS->u_lightDraws		= lightDrawCommands;
		generateLightsDrawCommnadsDS->u_lightDrawsCount	= lightDrawCommandsCount;

		lightsData.buildZClustersDS				= buildZClusters;
		lightsData.generateLightsDrawCommnadsDS	= generateLightsDrawCommnadsDS;
	}

	return lightsData;
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
}

void LightsRenderSystem::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	const LightsRenderingDataPerView lightsData = utils::CreateLightsRenderingData(graphBuilder, renderScene, viewSpec);
	
	viewSpec.GetData().Create<LightsRenderingDataPerView>(lightsData);
}

} // spt::rsc
