#include "LightsRenderSystem.h"
#include "RenderScene.h"
#include "LightTypes.h"
#include "View/ViewRenderingSpec.h"
#include "View/RenderView.h"
#include "ResourcesManager.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "Transfers/UploadUtils.h"

namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// LightsRenderSystem ============================================================================

BEGIN_SHADER_STRUCT(SceneLightsInfo)
	SHADER_STRUCT_FIELD(Uint32, localLightsNum)
	SHADER_STRUCT_FIELD(Uint32, localLights32Num)
	SHADER_STRUCT_FIELD(Uint32, directionalLightsNum)
END_SHADER_STRUCT();


struct LightsRenderingDataPerView
{
	lib::SharedPtr<rdr::Buffer> localLightsBuffer;
	lib::SharedPtr<rdr::Buffer> lightRangesBuffer;
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
	lightsData.localLightsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("SceneLocalLightsBuffer"), lightsBufferDefinition, lightsBuffersAllocationInfo);

	gfx::UploadDataToBufferOnCPU(lib::Ref(lightsData.localLightsBuffer), 0, reinterpret_cast<const Byte*>(pointLights.data()), pointLights.size() * sizeof(PointLightGPUData));

	const rhi::BufferDefinition lightRangesBufferDefinition(pointLightsZRanges.size() * sizeof(math::Vector2f), rhi::EBufferUsage::Storage);
	lightsData.lightRangesBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("SceneLightRangesBuffer"), lightsBufferDefinition, lightsBuffersAllocationInfo);

	gfx::UploadDataToBufferOnCPU(lib::Ref(lightsData.lightRangesBuffer), 0, reinterpret_cast<const Byte*>(pointLightsZRanges.data()), pointLightsZRanges.size() * sizeof(math::Vector2f));

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
