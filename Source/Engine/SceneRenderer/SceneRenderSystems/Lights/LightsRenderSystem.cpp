#include "LightsRenderSystem.h"
#include "RenderScene.h"
#include "Lights/LightTypes.h"
#include "SceneRenderSystems/ShadowMaps/ShadowMapsRenderSystem.h"
#include "Utils/ViewRenderingSpec.h"
#include "View/RenderView.h"
#include "ResourcesManager.h"
#include "ShaderStructs/ShaderStructs.h"
#include "Utils/TextureUtils.h"
#include "RGResources/RGResourceHandles.h"
#include "RenderGraphBuilder.h"
#include "Common/ShaderCompilationInput.h"
#include "ViewShadingInput.h"
#include "SceneRenderSystems/Atmosphere/AtmosphereRenderSystem.h"
#include "ViewRenderSystems/ParticipatingMedia/ParticipatingMediaViewRenderSystem.h"
#include "Bindless/BindlessTypes.h"
#include "Pipelines/PSOsLibraryTypes.h"
#include "Utils/TransfersUtils.h"
#include "RenderSceneConstants.h"

namespace spt::rsc
{

SPT_REGISTER_SCENE_RENDER_SYSTEM(LightsRenderSystem);

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


BEGIN_SHADER_STRUCT(BuildLightZClustersConstants)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<math::Vector2f>,   localLightsZRanges)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<LightsRenderingData>,      lightsData)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<math::Vector2u>, rwClusterRanges)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(GenerateLightDrawCommandsConstants)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<LocalLightGPUData>,       localLights)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<LightsRenderingData>,             lightsData)
	SHADER_STRUCT_FIELD(SceneViewCullingData,                         sceneViewCullingData)
	SHADER_STRUCT_FIELD(Uint32,                                       pointLightProxyVerticesNum)
	SHADER_STRUCT_FIELD(Uint32,                                       spotLightProxyVerticesNum)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<LightIndirectDrawCommand>, pointLightDraws)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<Uint32>,                   pointLightDrawsCount)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<LightIndirectDrawCommand>, spotLightDraws)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<Uint32>,                   spotLightDrawsCount)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(BuildLightTilesConstants)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<LocalLightGPUData>,        localLights)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<LightIndirectDrawCommand>, pointLightDraws)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<LightIndirectDrawCommand>, spotLightDraws)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<math::Vector3f>,        pointLightProxyVertices)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<math::Vector3f>,        spotLightProxyVertices)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<LightsRenderingData>,           lightsData)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<Uint32>,              tilesLightsMask)
END_SHADER_STRUCT();



BEGIN_SHADER_STRUCT(BuildLightsGridConstants)
	SHADER_STRUCT_FIELD(gfx::ConstTypedBufferRef<LocalLightGPUData>, localLights)
	SHADER_STRUCT_FIELD(Uint32,                                      localLightsNum)
	SHADER_STRUCT_FIELD(LightsGridCascades,                          cascades)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<Uint16>,               rwCells)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<Uint16>,               rwNodes)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<Uint32>,               rwNodesCounter)
END_SHADER_STRUCT();


SIMPLE_COMPUTE_PSO(BuildLightsGridPSO, "Sculptor/Lights/BuildLightsGrid.hlsl", BuildLightsGridCS);


BEGIN_RG_NODE_PARAMETERS_STRUCT(LightTilesIndirectDrawCommandsParameters)
	RG_BUFFER_VIEW(pointLightDrawCommandsBuffer,      rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
	RG_BUFFER_VIEW(pointLightDrawCommandsCountBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
	RG_BUFFER_VIEW(spotLightDrawCommandsBuffer,      rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
	RG_BUFFER_VIEW(spotLightDrawCommandsCountBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();


COMPUTE_PSO(BuildLightZClustersPSO)
{
	COMPUTE_SHADER("Sculptor/Lights/BuildLightsZClusters.hlsl", BuildLightsZClustersCS)

	PRESET(pso);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		pso = CompilePSO(compiler, { });
	}
};


COMPUTE_PSO(GenerateLightsDrawCommandsPSO)
{
	COMPUTE_SHADER("Sculptor/Lights/GenerateLightsDrawCommands.hlsl", GenerateLightsDrawCommandsCS)

	PRESET(pso);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		pso = CompilePSO(compiler, { });
	}
};


BEGIN_SHADER_STRUCT(BuildLightTilesPermutation)
	SHADER_STRUCT_FIELD(Int32, LIGHT_TYPE)
END_SHADER_STRUCT();


GRAPHICS_PSO(BuildLightTilesPSO)
{
	VERTEX_SHADER("Sculptor/Lights/BuildLightsTiles.hlsl", BuildLightsTilesVS)
	FRAGMENT_SHADER("Sculptor/Lights/BuildLightsTiles.hlsl", BuildLightsTilesPS)

	PRESET(pointLights);
	PRESET(spotLights);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		const rhi::GraphicsPipelineDefinition pipelineDef
		{
			.primitiveTopology       = rhi::EPrimitiveTopology::TriangleList,
			.rasterizationDefinition =
			{
				.cullMode = rhi::ECullMode::None,
				.rasterizationType = rhi::ERasterizationType::ConservativeOverestimate,
			}
		};

		pointLights = CompilePermutation(compiler, pipelineDef, BuildLightTilesPermutation{ .LIGHT_TYPE = 0u });
		spotLights  = CompilePermutation(compiler, pipelineDef, BuildLightTilesPermutation{ .LIGHT_TYPE = 1u });
	}
};


static lib::DynamicArray<math::Vector3f> CreateSpotLightProxyVertices(Uint32 baseEdges)
{
	const math::Vector3f apex       = math::Vector3f::Zero();
	const math::Vector3f baseCenter = math::Vector3f(1.f, 0.f, 0.f);

	const Real32 angleStep = (2.f * pi<Real32>) / static_cast<Real32>(baseEdges);

	const Real32 baseRadius = 1.f / std::cos(angleStep * 0.5f);

	lib::DynamicArray<math::Vector3f> vertices;

	for (Uint32 edgeIdx = 0; edgeIdx < baseEdges; ++edgeIdx)
	{
		const Real32 angle0 = angleStep * static_cast<Real32>(edgeIdx);
		const Real32 angle1 = angleStep * static_cast<Real32>(edgeIdx + 1u);

		const math::Vector3f baseVertex0 = baseCenter + math::Vector3f(0.f, std::cos(angle0) * baseRadius, std::sin(angle0) * baseRadius);
		const math::Vector3f baseVertex1 = baseCenter + math::Vector3f(0.f, std::cos(angle1) * baseRadius, std::sin(angle1) * baseRadius);

		vertices.emplace_back(apex);
		vertices.emplace_back(baseVertex0);
		vertices.emplace_back(baseVertex1);

		vertices.emplace_back(baseCenter);
		vertices.emplace_back(baseVertex0);
		vertices.emplace_back(baseVertex1);
	}

	return vertices;
}


static lib::DynamicArray<math::Vector3f> CreatePointLightProxyVertices()
{
	return {
		math::Vector3f(0.689107f, 0.689107f, 0.689107f), math::Vector3f(0.840115f, 0.000000f, 0.840115f), math::Vector3f(1.157020f, -0.000000f, 0.000000f), math::Vector3f(0.689107f, 0.689107f, 0.689107f), math::Vector3f(1.157020f, -0.000000f, 0.000000f),
		math::Vector3f(0.840115f, 0.840115f, -0.000000f), math::Vector3f(0.840115f, 0.000000f, 0.840115f), math::Vector3f(0.689107f, -0.689107f, 0.689107f), math::Vector3f(0.840115f, -0.840115f, 0.000000f), math::Vector3f(0.840115f, 0.000000f, 0.840115f),
		math::Vector3f(0.840115f, -0.840115f, 0.000000f), math::Vector3f(1.157020f, -0.000000f, 0.000000f), math::Vector3f(1.157020f, -0.000000f, 0.000000f), math::Vector3f(0.840115f, -0.840115f, 0.000000f), math::Vector3f(0.689107f, -0.689107f, -0.689107f),
		math::Vector3f(1.157020f, -0.000000f, 0.000000f), math::Vector3f(0.689107f, -0.689107f, -0.689107f), math::Vector3f(0.840117f, -0.000000f, -0.840117f), math::Vector3f(0.840115f, 0.840115f, -0.000000f), math::Vector3f(1.157020f, -0.000000f, 0.000000f),
		math::Vector3f(0.840117f, -0.000000f, -0.840117f), math::Vector3f(0.840115f, 0.840115f, -0.000000f), math::Vector3f(0.840117f, -0.000000f, -0.840117f), math::Vector3f(0.689107f, 0.689107f, -0.689107f), math::Vector3f(-0.689107f, 0.689107f, -0.689107f),
		math::Vector3f(0.000000f, 0.840117f, -0.840115f), math::Vector3f(0.000000f, 0.000000f, -1.157020f), math::Vector3f(-0.689107f, 0.689107f, -0.689107f), math::Vector3f(0.000000f, 0.000000f, -1.157020f), math::Vector3f(-0.840115f, -0.000000f, -0.840115f),
		math::Vector3f(0.000000f, 0.840117f, -0.840115f), math::Vector3f(0.689107f, 0.689107f, -0.689107f), math::Vector3f(0.840117f, -0.000000f, -0.840117f), math::Vector3f(0.000000f, 0.840117f, -0.840115f), math::Vector3f(0.840117f, -0.000000f, -0.840117f),
		math::Vector3f(0.000000f, 0.000000f, -1.157020f), math::Vector3f(0.000000f, 0.000000f, -1.157020f), math::Vector3f(0.840117f, -0.000000f, -0.840117f), math::Vector3f(0.689107f, -0.689107f, -0.689107f), math::Vector3f(0.000000f, 0.000000f, -1.157020f),
		math::Vector3f(0.689107f, -0.689107f, -0.689107f), math::Vector3f(-0.000000f, -0.840115f, -0.840117f), math::Vector3f(-0.840115f, -0.000000f, -0.840115f), math::Vector3f(0.000000f, 0.000000f, -1.157020f), math::Vector3f(-0.000000f, -0.840115f, -0.840117f),
		math::Vector3f(-0.840115f, -0.000000f, -0.840115f), math::Vector3f(-0.000000f, -0.840115f, -0.840117f), math::Vector3f(-0.689107f, -0.689107f, -0.689107f), math::Vector3f(-0.689107f, -0.689107f, -0.689107f), math::Vector3f(-0.000000f, -0.840115f, -0.840117f),
		math::Vector3f(0.000000f, -1.157020f, 0.000000f), math::Vector3f(-0.689107f, -0.689107f, -0.689107f), math::Vector3f(0.000000f, -1.157020f, 0.000000f), math::Vector3f(-0.840117f, -0.840115f, 0.000000f), math::Vector3f(-0.000000f, -0.840115f, -0.840117f),
		math::Vector3f(0.689107f, -0.689107f, -0.689107f), math::Vector3f(0.840115f, -0.840115f, 0.000000f), math::Vector3f(-0.000000f, -0.840115f, -0.840117f), math::Vector3f(0.840115f, -0.840115f, 0.000000f), math::Vector3f(0.000000f, -1.157020f, 0.000000f),
		math::Vector3f(0.000000f, -1.157020f, 0.000000f), math::Vector3f(0.840115f, -0.840115f, 0.000000f), math::Vector3f(0.689107f, -0.689107f, 0.689107f), math::Vector3f(0.000000f, -1.157020f, 0.000000f), math::Vector3f(0.689107f, -0.689107f, 0.689107f),
		math::Vector3f(-0.000000f, -0.840117f, 0.840115f), math::Vector3f(-0.840117f, -0.840115f, 0.000000f), math::Vector3f(0.000000f, -1.157020f, 0.000000f), math::Vector3f(-0.000000f, -0.840117f, 0.840115f), math::Vector3f(-0.840117f, -0.840115f, 0.000000f),
		math::Vector3f(-0.000000f, -0.840117f, 0.840115f), math::Vector3f(-0.689107f, -0.689107f, 0.689107f), math::Vector3f(-0.689107f, -0.689107f, 0.689107f), math::Vector3f(-0.840115f, 0.000000f, 0.840115f), math::Vector3f(-1.157020f, 0.000000f, -0.000000f),
		math::Vector3f(-0.689107f, -0.689107f, 0.689107f), math::Vector3f(-1.157020f, 0.000000f, -0.000000f), math::Vector3f(-0.840117f, -0.840115f, 0.000000f), math::Vector3f(-0.840115f, 0.000000f, 0.840115f), math::Vector3f(-0.689107f, 0.689108f, 0.689107f),
		math::Vector3f(-0.840115f, 0.840115f, -0.000000f), math::Vector3f(-0.840115f, 0.000000f, 0.840115f), math::Vector3f(-0.840115f, 0.840115f, -0.000000f), math::Vector3f(-1.157020f, 0.000000f, -0.000000f), math::Vector3f(-1.157020f, 0.000000f, -0.000000f),
		math::Vector3f(-0.840115f, 0.840115f, -0.000000f), math::Vector3f(-0.689107f, 0.689107f, -0.689107f), math::Vector3f(-1.157020f, 0.000000f, -0.000000f), math::Vector3f(-0.689107f, 0.689107f, -0.689107f), math::Vector3f(-0.840115f, -0.000000f, -0.840115f),
		math::Vector3f(-0.840117f, -0.840115f, 0.000000f), math::Vector3f(-1.157020f, 0.000000f, -0.000000f), math::Vector3f(-0.840115f, -0.000000f, -0.840115f), math::Vector3f(-0.840117f, -0.840115f, 0.000000f), math::Vector3f(-0.840115f, -0.000000f, -0.840115f),
		math::Vector3f(-0.689107f, -0.689107f, -0.689107f), math::Vector3f(-0.689107f, 0.689108f, 0.689107f), math::Vector3f(0.000000f, 0.840115f, 0.840117f), math::Vector3f(0.000000f, 1.157020f, 0.000000f), math::Vector3f(-0.689107f, 0.689108f, 0.689107f),
		math::Vector3f(0.000000f, 1.157020f, 0.000000f), math::Vector3f(-0.840115f, 0.840115f, -0.000000f), math::Vector3f(0.000000f, 0.840115f, 0.840117f), math::Vector3f(0.689107f, 0.689107f, 0.689107f), math::Vector3f(0.840115f, 0.840115f, -0.000000f),
		math::Vector3f(0.000000f, 0.840115f, 0.840117f), math::Vector3f(0.840115f, 0.840115f, -0.000000f), math::Vector3f(0.000000f, 1.157020f, 0.000000f), math::Vector3f(0.000000f, 1.157020f, 0.000000f), math::Vector3f(0.840115f, 0.840115f, -0.000000f),
		math::Vector3f(0.689107f, 0.689107f, -0.689107f), math::Vector3f(0.000000f, 1.157020f, 0.000000f), math::Vector3f(0.689107f, 0.689107f, -0.689107f), math::Vector3f(0.000000f, 0.840117f, -0.840115f), math::Vector3f(-0.840115f, 0.840115f, -0.000000f),
		math::Vector3f(0.000000f, 1.157020f, 0.000000f), math::Vector3f(0.000000f, 0.840117f, -0.840115f), math::Vector3f(-0.840115f, 0.840115f, -0.000000f), math::Vector3f(0.000000f, 0.840117f, -0.840115f), math::Vector3f(-0.689107f, 0.689107f, -0.689107f),
		math::Vector3f(-0.689107f, -0.689107f, 0.689107f), math::Vector3f(-0.000000f, -0.840117f, 0.840115f), math::Vector3f(0.000000f, 0.000000f, 1.157020f), math::Vector3f(-0.689107f, -0.689107f, 0.689107f), math::Vector3f(0.000000f, 0.000000f, 1.157020f),
		math::Vector3f(-0.840115f, 0.000000f, 0.840115f), math::Vector3f(-0.000000f, -0.840117f, 0.840115f), math::Vector3f(0.689107f, -0.689107f, 0.689107f), math::Vector3f(0.840115f, 0.000000f, 0.840115f), math::Vector3f(-0.000000f, -0.840117f, 0.840115f),
		math::Vector3f(0.840115f, 0.000000f, 0.840115f), math::Vector3f(0.000000f, 0.000000f, 1.157020f), math::Vector3f(0.000000f, 0.000000f, 1.157020f), math::Vector3f(0.840115f, 0.000000f, 0.840115f), math::Vector3f(0.689107f, 0.689107f, 0.689107f),
		math::Vector3f(0.000000f, 0.000000f, 1.157020f), math::Vector3f(0.689107f, 0.689107f, 0.689107f), math::Vector3f(0.000000f, 0.840115f, 0.840117f), math::Vector3f(-0.840115f, 0.000000f, 0.840115f), math::Vector3f(0.000000f, 0.000000f, 1.157020f),
		math::Vector3f(0.000000f, 0.840115f, 0.840117f), math::Vector3f(-0.840115f, 0.000000f, 0.840115f), math::Vector3f(0.000000f, 0.840115f, 0.840117f), math::Vector3f(-0.689107f, 0.689108f, 0.689107f)
	};
}


struct LightsRenderingDataPerView
{
	LightsRenderingDataPerView()
		: spotLightsToRenderNum(0)
		, pointLightsToRenderNum(0)
		, directionalLightsNum(0)
		, zClustersNum(0)
	{ }

	Uint32 GetLocalLightsToRenderNum() const
	{
		return spotLightsToRenderNum + pointLightsToRenderNum;
	}

	Bool HasAnyLocalLightsToRender() const
	{
		return GetLocalLightsToRenderNum() > 0;
	}

	Bool HasAnyDirectionalLightsToRender() const
	{
		return directionalLightsNum > 0;
	}

	Uint32 spotLightsToRenderNum;
	Uint32 pointLightsToRenderNum;
	Uint32 directionalLightsNum;

	math::Vector2u	tilesNum;
	Uint32			zClustersNum;

	rdr::GPUPtr<BuildLightZClustersConstants>       buildZClustersConstants;
	rdr::GPUPtr<GenerateLightDrawCommandsConstants> generateLightsDrawCommnadsConstants;
	rdr::GPUPtr<BuildLightTilesConstants>           buildLightTilesConstants;

	ViewShadingParams              viewShadingParamsData;
	rdr::GPUPtr<ViewShadingParams> viewShadingParams;

	rg::RGBufferViewHandle pointLightDrawCommandsBuffer;
	rg::RGBufferViewHandle pointLightDrawCommandsCountBuffer;

	rg::RGBufferViewHandle spotLightDrawCommandsBuffer;
	rg::RGBufferViewHandle spotLightDrawCommandsCountBuffer;
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// Utilities =====================================================================================

namespace utils
{

struct LightsInfo
{
	Uint32 pointLightsNum = 0u;
	Uint32 spotLightsNum  = 0u;
};

static LightsInfo CreateLocalLightsData(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, rg::RGBufferViewHandle& OUT localLightsRGBuffer, rg::RGBufferViewHandle& OUT lightZRangesRGBuffer)
{
	SPT_PROFILER_FUNCTION();

	const RenderView& renderView = viewSpec.GetRenderView();

	lib::DynamicArray<LocalLightGPUData> localLights;
	lib::DynamicArray<math::Vector2f> localLightsZRanges;

	const auto getViewSpaceZ = [ viewMatrix = renderView.GetViewRenderingData().viewMatrix ](const LocalLightGPUData& localLight)
	{
		math::Vector4f lightLocation = math::Vector4f::UnitW();
		lightLocation.head<3>() = localLight.location;
		const Real32 viewSpaceZ = viewMatrix.row(0).dot(lightLocation);
		return viewSpaceZ;
	};

	const SizeType maxLocalLightsNum = renderScene.lighting.pointLights.GetNum() + renderScene.lighting.spotLights.GetNum();

	lib::DynamicArray<Real32> localLightsViewSpaceZ;
	localLights.reserve(maxLocalLightsNum);
	localLightsViewSpaceZ.reserve(maxLocalLightsNum);

	const ShadowMapsRenderSystem& smSystem = rendererInterface.GetRenderSystemChecked<ShadowMapsRenderSystem>();
	const LocalLightsShadowMaps& shadowMaps =  smSystem.GetLocalLightsShadowMaps();

	auto shadowMapIt = shadowMaps.begin();

	Uint32 pointLightsNum = 0u;

	const auto processPointLight = [&](PointLightHandle handle, const PointLightData& pointLight)
	{
		LocalLightGPUData gpuLightData = GPUDataBuilder::CreatePointLightGPUData(pointLight);
		gpuLightData.shadowMapFirstFaceIdx = idxNone<Uint32>;

		if (shadowMapIt != shadowMaps.end() && shadowMapIt->first == handle)
		{
			gpuLightData.shadowMapFirstFaceIdx = shadowMapIt->second.shadowMapFirstFaceIdx;
			++shadowMapIt;
		}

		const Real32 lightViewSpaceZ = getViewSpaceZ(gpuLightData);

		if (lightViewSpaceZ + gpuLightData.range > 0.f)
		{
			const auto emplaceIt = std::upper_bound(std::cbegin(localLightsViewSpaceZ), std::cend(localLightsViewSpaceZ), lightViewSpaceZ);
			const SizeType emplaceIdx = std::distance(std::cbegin(localLightsViewSpaceZ), emplaceIt);

			localLightsViewSpaceZ.emplace(emplaceIt, lightViewSpaceZ);
			localLights.emplace(std::cbegin(localLights) + emplaceIdx, gpuLightData);

			++pointLightsNum;
		}
	};

	Uint32 spotLightsNum = 0u;

	const auto processSpotLight = [&](SpotLightHandle handle, const SpotLightData& spotLight)
	{
		LocalLightGPUData gpuLightData = GPUDataBuilder::CreateSpotLightGPUData(spotLight);
		gpuLightData.shadowMapFirstFaceIdx = idxNone<Uint32>;

		const Real32 lightViewSpaceZ = getViewSpaceZ(gpuLightData);
		if (lightViewSpaceZ + gpuLightData.range > 0.f)
		{
			const auto emplaceIt = std::upper_bound(std::cbegin(localLightsViewSpaceZ), std::cend(localLightsViewSpaceZ), lightViewSpaceZ);
			const SizeType emplaceIdx = std::distance(std::cbegin(localLightsViewSpaceZ), emplaceIt);
			localLightsViewSpaceZ.emplace(emplaceIt, lightViewSpaceZ);
			localLights.emplace(std::cbegin(localLights) + emplaceIdx, gpuLightData);

			++spotLightsNum;
		}
	};

	renderScene.lighting.pointLights.ForEach(processPointLight);
	renderScene.lighting.spotLights.ForEach(processSpotLight);

	for (SizeType idx = 0; idx < localLights.size(); ++idx)
	{
		localLightsZRanges.emplace_back(math::Vector2f(localLightsViewSpaceZ[idx] - localLights[idx].range, localLightsViewSpaceZ[idx] + localLights[idx].range));
	}

	if (!localLights.empty())
	{
		lib::DynamicArray<rdr::HLSLStorage<LocalLightGPUData>> hlslLocalLights(localLights.size());
		for (SizeType idx = 0; idx < localLights.size(); ++idx)
		{
			hlslLocalLights[idx] = localLights[idx];
		}

		const rhi::BufferDefinition lightsBufferDefinition(localLights.size() * sizeof(rdr::HLSLStorage<LocalLightGPUData>), rhi::EBufferUsage::Storage);
		const lib::SharedRef<rdr::Buffer> localLightsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("SceneLocalLights"), lightsBufferDefinition, rhi::EMemoryUsage::CPUToGPU);
		rdr::UploadDataToBuffer(localLightsBuffer, 0, reinterpret_cast<const Byte*>(hlslLocalLights.data()), hlslLocalLights.size() * sizeof(rdr::HLSLStorage<LocalLightGPUData>));
		localLightsRGBuffer = graphBuilder.AcquireExternalBufferView(localLightsBuffer->GetFullView());

		graphBuilder.AcquireExternalBufferView(localLightsBuffer->GetFullView());

		const rhi::BufferDefinition lightZRangesBufferDefinition(localLightsZRanges.size() * sizeof(math::Vector2f), rhi::EBufferUsage::Storage);
		const lib::SharedRef<rdr::Buffer> lightZRangesBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("SceneLightZRanges"), lightZRangesBufferDefinition, rhi::EMemoryUsage::CPUToGPU);
		rdr::UploadDataToBuffer(lightZRangesBuffer, 0, reinterpret_cast<const Byte*>(localLightsZRanges.data()), localLightsZRanges.size() * sizeof(math::Vector2f));
		lightZRangesRGBuffer = graphBuilder.AcquireExternalBufferView(lightZRangesBuffer->GetFullView());

		graphBuilder.AcquireExternalBufferView(lightZRangesBuffer->GetFullView());
	}

	return LightsInfo
	{
		.pointLightsNum = pointLightsNum,
		.spotLightsNum  = spotLightsNum
	};
}

struct LightsRenderingParams
{
	lib::SharedPtr<rdr::Buffer> pointLightProxyVerticesBuffer;
	lib::SharedPtr<rdr::Buffer> spotLightProxyVerticesBuffer;

	Uint32 pointLightProxyVerticesNum = 0u;
	Uint32 spotLightProxyVerticesNum = 0u;
};


static LightsRenderingDataPerView CreateLightsRenderingData(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const LightsRenderingParams& renderingParams)
{
	SPT_PROFILER_FUNCTION();

	const Uint32 zClustersNum = 256u;

	LightsRenderingDataPerView lightsRenderingDataPerView;

	LightsRenderingData lightsData;
	rdr::GPUPtr<LightsRenderingData> lightsDataPtr = graphBuilder.CreateGPUData<LightsRenderingData>();
	
	rg::RGBufferViewHandle localLightsBuffer;
	rg::RGBufferViewHandle localLightsZRangesBuffer;
	const LightsInfo lightsInfo = CreateLocalLightsData(graphBuilder, rendererInterface, renderScene, viewSpec, OUT localLightsBuffer, OUT localLightsZRangesBuffer);
	const Uint32 localLightsNum = lightsInfo.pointLightsNum + lightsInfo.spotLightsNum;

	lightsRenderingDataPerView.spotLightsToRenderNum  = lightsInfo.spotLightsNum;
	lightsRenderingDataPerView.pointLightsToRenderNum = lightsInfo.pointLightsNum;

	ViewShadingParams viewShadingParams;

	if (lightsRenderingDataPerView.HasAnyLocalLightsToRender())
	{
		const math::Vector2u renderingRes = viewSpec.GetRenderingRes();
		const math::Vector2u tilesNum = (renderingRes - math::Vector2u(1, 1)) / 8 + math::Vector2u(1, 1);
		const math::Vector2f tileSize = math::Vector2f(1.f / static_cast<Real32>(tilesNum.x()), 1.f / static_cast<Real32>(tilesNum.y()));

		lightsData.localLightsNum   = localLightsNum;
		lightsData.localLights32Num = math::Utils::DivideCeil(localLightsNum, 32u);
		lightsData.zClusterLength   = viewSpec.GetSettings().shading.lightsZClusterLength;
		lightsData.zClustersNum     = zClustersNum;
		lightsData.tilesNum         = tilesNum;
		lightsData.tileSize         = tileSize;

		rg::RGBufferViewHandle pointLightDrawCommands;
		rg::RGBufferViewHandle pointLightDrawCommandsCount;

		rg::RGBufferViewHandle spotLightDrawCommands;
		rg::RGBufferViewHandle spotLightDrawCommandsCount;

		if (lightsInfo.pointLightsNum > 0u)
		{
			const rhi::BufferDefinition pointLightDrawCommandsBufferDefinition(lightsInfo.pointLightsNum * rdr::shader_translator::HLSLSizeOf<LightIndirectDrawCommand>(), lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect));
			pointLightDrawCommands = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Point Light Draw Commands"), pointLightDrawCommandsBufferDefinition, rhi::EMemoryUsage::GPUOnly);

			const rhi::BufferDefinition pointLightDrawCommandsCountBufferDefinition(sizeof(Uint32), lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect, rhi::EBufferUsage::TransferDst));
			pointLightDrawCommandsCount = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Point Light Draw Commands Count"), pointLightDrawCommandsCountBufferDefinition, rhi::EMemoryUsage::GPUOnly);
			graphBuilder.FillBuffer(RG_DEBUG_NAME("Initialize Point Light Draw Commands Count"), pointLightDrawCommandsCount, 0, sizeof(Uint32), 0);
		}

		if (lightsInfo.spotLightsNum > 0u)
		{
			const rhi::BufferDefinition spotLightDrawCommandsBufferDefinition(lightsInfo.spotLightsNum * rdr::shader_translator::HLSLSizeOf<LightIndirectDrawCommand>(), lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect));
			spotLightDrawCommands = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Spot Light Draw Commands"), spotLightDrawCommandsBufferDefinition, rhi::EMemoryUsage::GPUOnly);

			const rhi::BufferDefinition spotLightDrawCommandsCountBufferDefinition(sizeof(Uint32), lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect, rhi::EBufferUsage::TransferDst));
			spotLightDrawCommandsCount = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Spot Light Draw Commands Count"), spotLightDrawCommandsCountBufferDefinition, rhi::EMemoryUsage::GPUOnly);
			graphBuilder.FillBuffer(RG_DEBUG_NAME("Initialize Spot Light Draw Commands Count"), spotLightDrawCommandsCount, 0, sizeof(Uint32), 0);
		}

		const rhi::BufferDefinition clustersRangesBufferDefinition(zClustersNum * sizeof(math::Vector2u), lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst));
		const rg::RGBufferViewHandle clustersRanges = graphBuilder.CreateBufferView(RG_DEBUG_NAME("ClustersRanges"), clustersRangesBufferDefinition, rhi::EMemoryUsage::GPUOnly);

		const Uint32 tilesLightsMaskBufferSize = tilesNum.x() * tilesNum.y() * lightsData.localLights32Num * sizeof(Uint32);
		const rhi::BufferDefinition tilesLightsMaskDefinition(tilesLightsMaskBufferSize, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst));
		const rg::RGBufferViewHandle tilesLightsMask = graphBuilder.CreateBufferView(RG_DEBUG_NAME("TilesLightsMask"), tilesLightsMaskDefinition, rhi::EMemoryUsage::GPUOnly);
		graphBuilder.FillBuffer(RG_DEBUG_NAME("InitializeTilesLightsMask"), tilesLightsMask, 0, static_cast<Uint64>(tilesLightsMaskBufferSize), 0);

		BuildLightZClustersConstants buildClustersConstants;
		buildClustersConstants.localLightsZRanges = localLightsZRangesBuffer;
		buildClustersConstants.lightsData         = lightsDataPtr;
		buildClustersConstants.rwClusterRanges    = clustersRanges;

		const SceneViewCullingData& cullingData	= viewSpec.GetRenderView().GetCullingData();

		GenerateLightDrawCommandsConstants generateLightsDrawCommnadsConstants;
		generateLightsDrawCommnadsConstants.localLights                = localLightsBuffer;
		generateLightsDrawCommnadsConstants.lightsData                 = lightsDataPtr;
		generateLightsDrawCommnadsConstants.pointLightProxyVerticesNum = renderingParams.pointLightProxyVerticesNum;
		generateLightsDrawCommnadsConstants.spotLightProxyVerticesNum  = renderingParams.spotLightProxyVerticesNum;
		generateLightsDrawCommnadsConstants.sceneViewCullingData       = cullingData;
		generateLightsDrawCommnadsConstants.pointLightDraws            = pointLightDrawCommands;
		generateLightsDrawCommnadsConstants.pointLightDrawsCount       = pointLightDrawCommandsCount;
		generateLightsDrawCommnadsConstants.spotLightDraws             = spotLightDrawCommands;
		generateLightsDrawCommnadsConstants.spotLightDrawsCount        = spotLightDrawCommandsCount;

		BuildLightTilesConstants buildLightTilesConstants;
		buildLightTilesConstants.localLights              = localLightsBuffer;
		buildLightTilesConstants.pointLightDraws          = pointLightDrawCommands;
		buildLightTilesConstants.spotLightDraws           = spotLightDrawCommands;
		buildLightTilesConstants.pointLightProxyVertices  = renderingParams.pointLightProxyVerticesBuffer->GetFullView();
		buildLightTilesConstants.spotLightProxyVertices   = renderingParams.spotLightProxyVerticesBuffer->GetFullView();
		buildLightTilesConstants.lightsData               = lightsDataPtr;
		buildLightTilesConstants.tilesLightsMask          = tilesLightsMask;

		viewShadingParams.localLights     = localLightsBuffer;
		viewShadingParams.tilesLightsMask = tilesLightsMask;
		viewShadingParams.clustersRanges  = clustersRanges;

		lightsRenderingDataPerView.buildZClustersConstants             = graphBuilder.CreateGPUData(buildClustersConstants);
		lightsRenderingDataPerView.generateLightsDrawCommnadsConstants = graphBuilder.CreateGPUData(generateLightsDrawCommnadsConstants);
		lightsRenderingDataPerView.buildLightTilesConstants            = graphBuilder.CreateGPUData(buildLightTilesConstants);

		lightsRenderingDataPerView.pointLightDrawCommandsBuffer      = pointLightDrawCommands;
		lightsRenderingDataPerView.pointLightDrawCommandsCountBuffer = pointLightDrawCommandsCount;

		lightsRenderingDataPerView.spotLightDrawCommandsBuffer      = spotLightDrawCommands;
		lightsRenderingDataPerView.spotLightDrawCommandsCountBuffer = spotLightDrawCommandsCount;
	}

	const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();
	SPT_CHECK(viewContext.dirLightShadowMask.IsValid());

	viewShadingParams.shadowMask              = viewContext.dirLightShadowMask;
	viewShadingParams.ambientOcclusionTexture = viewContext.ambientOcclusion;
	viewShadingParams.lightsData              = lightsDataPtr;
	
	lightsRenderingDataPerView.viewShadingParams = graphBuilder.CreateGPUData(viewShadingParams);

	lightsRenderingDataPerView.tilesNum		= lightsData.tilesNum;
	lightsRenderingDataPerView.zClustersNum	= lightsData.zClustersNum;

	lightsDataPtr.SetData(lightsData);

	return lightsRenderingDataPerView;
}

} // utils

//////////////////////////////////////////////////////////////////////////////////////////////////
// LightsRenderSystem ============================================================================

LightsRenderSystem::LightsRenderSystem(lib::MemoryArena& arena, RenderScene& owningScene)
	: Super(arena, owningScene)
{
	m_supportedStages = lib::Flags(ERenderStage::ForwardOpaque, ERenderStage::DeferredShading);

	const lib::DynamicArray<math::Vector3f> spotLightProxyVertices = CreateSpotLightProxyVertices(12u);
	rhi::BufferDefinition spotLightProxyVerticesBufferDef(spotLightProxyVertices.size() * sizeof(math::Vector3f), lib::Flags(rhi::EBufferUsage::TransferDst, rhi::EBufferUsage::Storage));
	m_spotLightProxyVertices = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Spot Light Proxy Vertices"), spotLightProxyVerticesBufferDef, rhi::EMemoryUsage::GPUOnly);

	const lib::DynamicArray<math::Vector3f> pointLightProxyVertices = CreatePointLightProxyVertices();
	rhi::BufferDefinition pointLightProxyVerticesBufferDef(pointLightProxyVertices.size() * sizeof(math::Vector3f), lib::Flags(rhi::EBufferUsage::TransferDst, rhi::EBufferUsage::Storage));
	m_pointLightProxyVertices = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Point Light Proxy Vertices"), pointLightProxyVerticesBufferDef, rhi::EMemoryUsage::GPUOnly);

	rdr::UploadDataToBuffer(lib::Ref(m_spotLightProxyVertices), 0u, reinterpret_cast<const Byte*>(spotLightProxyVertices.data()), spotLightProxyVertices.size() * sizeof(math::Vector3f));
	rdr::UploadDataToBuffer(lib::Ref(m_pointLightProxyVertices), 0u, reinterpret_cast<const Byte*>(pointLightProxyVertices.data()), pointLightProxyVertices.size() * sizeof(math::Vector3f));

	const rhi::BufferDefinition gridCellsDef(lights_grid_constants::cascadesNum * lights_grid_constants::cascadeCellsNum * sizeof(Uint16), lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst));
	m_lightsGridCells = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Lights Grid Cells"), gridCellsDef, rhi::EMemoryUsage::GPUOnly);

	const rhi::BufferDefinition gridNodesDef(lights_grid_constants::nodesCapacity * sizeof(Uint16), lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst));
	m_lightsGridNodes = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Lights Grid Nodes"), gridNodesDef, rhi::EMemoryUsage::GPUOnly);
}

void LightsRenderSystem::UpdateGPUSceneData(const SceneUpdateContext& context, RenderSceneConstants& sceneData)
{
	SPT_PROFILER_FUNCTION();

	Super::UpdateGPUSceneData(context, sceneData);

	const RenderScene& scene = GetOwningScene();

	const SizeType localLightsNum = scene.lighting.pointLights.GetNum() + scene.lighting.spotLights.GetNum();

	// Buffer is bound even if there are no local lights, so it can't be empty
	const Uint64 localLightsDataSize = std::max<SizeType>(localLightsNum, 1u) * sizeof(rdr::HLSLStorage<LocalLightGPUData>);
	m_sceneLocalLightsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Scene Local Lights Buffer"), rhi::BufferDefinition(localLightsDataSize, rhi::EBufferUsage::Storage), rhi::EMemoryUsage::CPUToGPU);
	{
		rhi::RHIMappedBuffer<rdr::HLSLStorage<LocalLightGPUData>> localLightsData(m_sceneLocalLightsBuffer->GetRHI());

		SizeType localLightIdx = 0;

		const ShadowMapsRenderSystem& smSystem  = context.rendererInterface.GetRenderSystemChecked<ShadowMapsRenderSystem>();
		const LocalLightsShadowMaps& shadowMaps = smSystem.GetLocalLightsShadowMaps();

		auto shadowMapIt = shadowMaps.begin();

		const auto processPointLight = [&](PointLightHandle handle, const PointLightData& pointLight)
		{
			LocalLightGPUData gpuLightData = GPUDataBuilder::CreatePointLightGPUData(pointLight);
			gpuLightData.shadowMapFirstFaceIdx = idxNone<Uint32>;

			if (shadowMapIt != shadowMaps.end() && shadowMapIt->first == handle)
			{
				gpuLightData.shadowMapFirstFaceIdx = shadowMapIt->second.shadowMapFirstFaceIdx;
				++shadowMapIt;
			}

			localLightsData[localLightIdx++] = gpuLightData;
		};

		const auto processSpotLight = [&](SpotLightHandle handle, const SpotLightData& spotLight)
		{
			LocalLightGPUData gpuLightData = GPUDataBuilder::CreateSpotLightGPUData(spotLight);
			gpuLightData.shadowMapFirstFaceIdx = idxNone<Uint32>;

			localLightsData[localLightIdx++] = gpuLightData;
		};

		scene.lighting.pointLights.ForEach(processPointLight);
		scene.lighting.spotLights.ForEach(processSpotLight);

		SPT_CHECK(localLightIdx == localLightsNum);
	}

	const AtmosphereRenderSystem& atmosphereSystem = context.rendererInterface.GetRenderSystemChecked<AtmosphereRenderSystem>();
	const AtmosphereContext& atmosphereContext     = atmosphereSystem.GetAtmosphereContext();
	SPT_CHECK(!!atmosphereContext.directionalLightsBuffer);

	SceneLightsData lightsData;
	lightsData.heightFog                      = ParticipatingMediaViewRenderSystem::CreateHeightFogParams(scene.settings.fog);
	lightsData.localLightsNum                 = static_cast<Uint32>(localLightsNum);
	lightsData.directionalLightsNum           = atmosphereSystem.GetAtmosphereParams().directionalLightsNum;
	lightsData.localLights                    = m_sceneLocalLightsBuffer->GetFullView();
	lightsData.directionalLights              = atmosphereContext.directionalLightsBuffer->GetFullView();
	lightsData.hasValidCloudsTransmittanceMap = atmosphereSystem.AreVolumetricCloudsEnabled();

	if (lightsData.hasValidCloudsTransmittanceMap)
	{
		lightsData.cloudsTransmittanceViewProj = atmosphereSystem.ComputeCloudsTransmittanceMapViewProj();
		lightsData.cloudsTransmittanceMap      = atmosphereSystem.GetCloudsTransmittanceTexture();
	}

	const math::Vector3f gridCenter = context.mainRenderView.GetLocation();
	const math::Vector3f cascadeRes = math::Vector3f(lights_grid_constants::cascadeResXY, lights_grid_constants::cascadeResXY, lights_grid_constants::cascadeResZ);

	for (Uint32 cascadeIdx = 0u; cascadeIdx < lights_grid_constants::cascadesNum; ++cascadeIdx)
	{
		LightsGridCascade& cascade = m_lightsGridCascades[cascadeIdx];
		cascade.cellSize = lights_grid_constants::baseCellSize * static_cast<Real32>(1u << cascadeIdx);
		cascade.origin   = (gridCenter / cascade.cellSize).array().floor().matrix() * cascade.cellSize - cascadeRes * cascade.cellSize * 0.5f;
	}

	m_localLightsNum = static_cast<Uint32>(localLightsNum);

	lightsData.lightsGrid.cascades = m_lightsGridCascades;
	lightsData.lightsGrid.cells    = m_lightsGridCells->GetFullView();
	lightsData.lightsGrid.nodes    = m_lightsGridNodes->GetFullView();

	sceneData.lights = lightsData;
}

void LightsRenderSystem::CollectRenderViews(const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const RenderView& mainRenderView, INOUT RenderViewsCollector& viewsCollector)
{
	SPT_PROFILER_FUNCTION();

	Super::CollectRenderViews(rendererInterface, renderScene, mainRenderView, viewsCollector);

	if (ShadowMapsRenderSystem* shadowMapsRenderSystem = rendererInterface.GetRenderSystem<ShadowMapsRenderSystem>())
	{
		const lib::DynamicArray<RenderView*> shadowMapViewsToRender = shadowMapsRenderSystem->GetShadowMapViewsToUpdate();
		for (RenderView* renderView : shadowMapViewsToRender)
		{
			SPT_CHECK(!!renderView);
			viewsCollector.AddRenderView(*renderView);
		}
	}
}

void LightsRenderSystem::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const lib::DynamicPushArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings)
{
	Super::RenderPerFrame(graphBuilder, rendererInterface, renderScene, viewSpecs, settings);

	BuildLightsGrid(graphBuilder);

	for (ViewRenderingSpec* viewSpec : viewSpecs)
	{
		SPT_CHECK(!!viewSpec);
		RenderPerView(graphBuilder, renderScene, *viewSpec);
	}
}

void LightsRenderSystem::BuildLightsGrid(rg::RenderGraphBuilder& graphBuilder)
{
	SPT_PROFILER_FUNCTION();

	const rg::RGBufferViewHandle cells        = graphBuilder.AcquireExternalBufferView(m_lightsGridCells->GetFullView());
	const rg::RGBufferViewHandle nodes        = graphBuilder.AcquireExternalBufferView(m_lightsGridNodes->GetFullView());
	const rg::RGBufferViewHandle nodesCounter = graphBuilder.CreateStorageBufferView(RG_DEBUG_NAME("Lights Grid Nodes Counter"), sizeof(Uint32));

	// Nodes are allocated with atomics
	cells->SetNonDeterministic();
	nodes->SetNonDeterministic();

	// Nodes 0 and 1 are terminators shared by all empty cells
	graphBuilder.FillBuffer(RG_DEBUG_NAME("Init Lights Grid Empty List"), nodes, 0u, sizeof(Uint32), 0xFFFFFFFFu);
	graphBuilder.FillBuffer(RG_DEBUG_NAME("Init Lights Grid Nodes Counter"), nodesCounter, 0u, sizeof(Uint32), 2u);

	BuildLightsGridConstants shaderConstants;
	shaderConstants.localLights    = m_sceneLocalLightsBuffer->GetFullView();
	shaderConstants.localLightsNum = m_localLightsNum;
	shaderConstants.cascades       = m_lightsGridCascades;
	shaderConstants.rwCells        = cells;
	shaderConstants.rwNodes        = nodes;
	shaderConstants.rwNodesCounter = nodesCounter;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Build Lights Grid"),
						  BuildLightsGridPSO::pso,
						  math::Vector3u(lights_grid_constants::cascadeResXY, lights_grid_constants::cascadeResXY, lights_grid_constants::cascadeResZ * lights_grid_constants::cascadesNum),
						  shaderConstants);
}

void LightsRenderSystem::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	viewSpec.GetRenderViewEntry(ERenderViewEntry::BuildLightsTiles).AddRawMember(this, &LightsRenderSystem::BuildLightsTiles);
}

void LightsRenderSystem::BuildLightsTiles(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context)
{
	SPT_PROFILER_FUNCTION();

	ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	utils::LightsRenderingParams lightRenderingParams;
	lightRenderingParams.pointLightProxyVerticesBuffer = m_pointLightProxyVertices;
	lightRenderingParams.spotLightProxyVerticesBuffer  = m_spotLightProxyVertices;
	lightRenderingParams.pointLightProxyVerticesNum    = static_cast<Uint32>(m_pointLightProxyVertices->GetSize() / sizeof(math::Vector3f));
	lightRenderingParams.spotLightProxyVerticesNum     = static_cast<Uint32>(m_spotLightProxyVertices->GetSize() / sizeof(math::Vector3f));
	
	const LightsRenderingDataPerView lightsRenderingData = utils::CreateLightsRenderingData(graphBuilder, rendererInterface, renderScene, viewSpec, lightRenderingParams);

	if (lightsRenderingData.HasAnyLocalLightsToRender())
	{
		const math::Vector3u dispatchZClustersGroupsNum = math::Vector3u(math::Utils::DivideCeil<Uint32>(lightsRenderingData.zClustersNum, 32), 1, 1);
		graphBuilder.Dispatch(RG_DEBUG_NAME("BuildLightsZClusters"),
							  BuildLightZClustersPSO::pso,
							  dispatchZClustersGroupsNum,
							  rg::ShaderParams(lightsRenderingData.buildZClustersConstants));

		const math::Vector3u dispatchLightsGroupsNum = math::Vector3u(math::Utils::DivideCeil<Uint32>(lightsRenderingData.GetLocalLightsToRenderNum(), 32), 1, 1);
		graphBuilder.Dispatch(RG_DEBUG_NAME("GenerateLightsDrawCommands"),
							  GenerateLightsDrawCommandsPSO::pso,
							  dispatchLightsGroupsNum,
							  rg::ShaderParams(lightsRenderingData.generateLightsDrawCommnadsConstants, viewContext.depthCullingGPUData));

		const math::Vector2u renderingArea = lightsRenderingData.tilesNum;

		// No render targets - we only use pixel shader to write to UAV
		const rg::RGRenderPassDefinition lightsTilesRenderPassDef(math::Vector2i::Zero(), renderingArea);

		LightTilesIndirectDrawCommandsParameters passIndirectParams;
		passIndirectParams.pointLightDrawCommandsBuffer      = lightsRenderingData.pointLightDrawCommandsBuffer;
		passIndirectParams.pointLightDrawCommandsCountBuffer = lightsRenderingData.pointLightDrawCommandsCountBuffer;
		passIndirectParams.spotLightDrawCommandsBuffer       = lightsRenderingData.spotLightDrawCommandsBuffer;
		passIndirectParams.spotLightDrawCommandsCountBuffer  = lightsRenderingData.spotLightDrawCommandsCountBuffer;

		const Uint32 maxPointLightDrawsCount = lightsRenderingData.pointLightsToRenderNum;
		const Uint32 maxSpotLightDrawsCount  = lightsRenderingData.spotLightsToRenderNum;

		graphBuilder.RenderPass(RG_DEBUG_NAME("Build Lights Tiles"),
								lightsTilesRenderPassDef,
								rg::ShaderParams(lightsRenderingData.buildLightTilesConstants),
								std::tie(passIndirectParams),
								[this, passIndirectParams, maxPointLightDrawsCount, maxSpotLightDrawsCount, renderingArea](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
								{
									recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), renderingArea.cast<Real32>()), 0.f, 1.f);
									recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), renderingArea));

									if (maxPointLightDrawsCount > 0u)
									{
										const rdr::BufferView& pointLightsDawsBufferView      = *passIndirectParams.pointLightDrawCommandsBuffer->GetResource();
										const rdr::BufferView& pointLightsDrawCountBufferView = *passIndirectParams.pointLightDrawCommandsCountBuffer->GetResource();

										recorder.BindGraphicsPipeline(BuildLightTilesPSO::pointLights);
										recorder.DrawIndirectCount(pointLightsDawsBufferView, 0u, sizeof(rdr::HLSLStorage<LightIndirectDrawCommand>), pointLightsDrawCountBufferView, 0, maxPointLightDrawsCount);
									}

									if (maxSpotLightDrawsCount > 0u)
									{
										const rdr::BufferView& spotLightsDawsBufferView      = *passIndirectParams.spotLightDrawCommandsBuffer->GetResource();
										const rdr::BufferView& spotLightsDrawCountBufferView = *passIndirectParams.spotLightDrawCommandsCountBuffer->GetResource();

										recorder.BindGraphicsPipeline(BuildLightTilesPSO::spotLights);
										recorder.DrawIndirectCount(spotLightsDawsBufferView, 0u, sizeof(rdr::HLSLStorage<LightIndirectDrawCommand>), spotLightsDrawCountBufferView, 0, maxSpotLightDrawsCount);
									}
								});
	}

	viewContext.viewShadingParams = lightsRenderingData.viewShadingParams;

	ViewSpecShadingParameters shadingParams;
	shadingParams.viewShadingParams = lightsRenderingData.viewShadingParams;
	viewSpec.GetBlackboard().Create<ViewSpecShadingParameters>(shadingParams);
}

} // spt::rsc
