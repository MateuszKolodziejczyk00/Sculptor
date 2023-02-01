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


DS_BEGIN(BuildLightZClustersDS, rg::RGDescriptorSetState<BuildLightZClustersDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<math::Vector2f>),	u_localLightsRanges)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneLightsInfo>),	u_sceneLightsInfo)
DS_END();


DS_BEGIN(BuildLightTilesDS, rg::RGDescriptorSetState<BuildLightTilesDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<PointLightGPUData>),	u_localLights)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneLightsInfo>),		u_sceneLightsInfo)
DS_END();


struct LightsRenderingDataPerView
{
	lib::SharedPtr<BuildLightZClustersDS>	buildZClustersDS;
	lib::SharedPtr<BuildLightTilesDS>		buildTilesDS;
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// Utilities =====================================================================================

namespace utils
{

static LightsRenderingDataPerView CreateLightsRenderingData(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

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
	
	const rhi::RHIAllocationInfo lightsBuffersAllocationInfo(rhi::EMemoryUsage::CPUToGPU);

	const rhi::BufferDefinition lightsBufferDefinition(pointLights.size() * sizeof(PointLightGPUData), rhi::EBufferUsage::Storage);
	const lib::SharedRef<rdr::Buffer> localLightsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("SceneLocalLightsBuffer"), lightsBufferDefinition, lightsBuffersAllocationInfo);

	gfx::UploadDataToBufferOnCPU(localLightsBuffer, 0, reinterpret_cast<const Byte*>(pointLights.data()), pointLights.size() * sizeof(PointLightGPUData));

	const rhi::BufferDefinition lightRangesBufferDefinition(pointLightsZRanges.size() * sizeof(math::Vector2f), rhi::EBufferUsage::Storage);
	const lib::SharedRef<rdr::Buffer> lightRangesBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("SceneLightRangesBuffer"), lightsBufferDefinition, lightsBuffersAllocationInfo);

	gfx::UploadDataToBufferOnCPU(lightRangesBuffer, 0, reinterpret_cast<const Byte*>(pointLightsZRanges.data()), pointLightsZRanges.size() * sizeof(math::Vector2f));

	SceneLightsInfo lightsInfo;
	lightsInfo.localLightsNum		= static_cast<Uint32>(pointLights.size());
	lightsInfo.localLights32Num		= static_cast<Uint32>((pointLights.size() - 1) / 32 + 1);
	lightsInfo.directionalLightsNum = 0;
	lightsInfo.zClusterLength		= 20.f;
	lightsInfo.zClustersNum			= 8;

	const lib::SharedRef<BuildLightZClustersDS> buildZClusters = rdr::ResourcesManager::CreateDescriptorSetState<BuildLightZClustersDS>(RENDERER_RESOURCE_NAME("BuildLightZClustersDS"));
	buildZClusters->u_localLightsRanges	= lightRangesBuffer->CreateFullView();
	buildZClusters->u_sceneLightsInfo	= lightsInfo;
	
	const lib::SharedRef<BuildLightTilesDS> buildTiles = rdr::ResourcesManager::CreateDescriptorSetState<BuildLightTilesDS>(RENDERER_RESOURCE_NAME("BuildLightTilesDS"));
	buildTiles->u_localLights		= localLightsBuffer->CreateFullView();
	buildTiles->u_sceneLightsInfo	= lightsInfo;

	lightsData.buildZClustersDS = buildZClusters;
	lightsData.buildTilesDS		= buildTiles;

	return lightsData;
}

} // utils

//////////////////////////////////////////////////////////////////////////////////////////////////
// LightsRenderSystem ============================================================================

LightsRenderSystem::LightsRenderSystem()
{
	m_supportedStages = ERenderStage::ForwardOpaque;
}

void LightsRenderSystem::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	const LightsRenderingDataPerView lightsData = utils::CreateLightsRenderingData(graphBuilder, renderScene, viewSpec);
	
	viewSpec.GetData().Create<LightsRenderingDataPerView>(lightsData);
}

} // spt::rsc
