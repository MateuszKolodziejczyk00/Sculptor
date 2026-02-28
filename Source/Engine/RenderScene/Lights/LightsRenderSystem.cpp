#include "LightsRenderSystem.h"
#include "RenderScene.h"
#include "LightTypes.h"
#include "Shadows/ShadowMapsRenderSystem.h"
#include "View/ViewRenderingSpec.h"
#include "View/RenderView.h"
#include "ResourcesManager.h"
#include "ShaderStructs/ShaderStructs.h"
#include "Utils/TextureUtils.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "RGResources/RGResourceHandles.h"
#include "RenderGraphBuilder.h"
#include "Common/ShaderCompilationInput.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "ViewShadingInput.h"
#include "SceneRenderer/Utils/BRDFIntegrationLUT.h"
#include "Atmosphere/AtmosphereRenderSystem.h"
#include "ParticipatingMedia/ParticipatingMediaViewRenderSystem.h"
#include "Bindless/BindlessTypes.h"
#include "Pipelines/PSOsLibraryTypes.h"
#include "Utils/TransfersUtils.h"

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


BEGIN_SHADER_STRUCT(BuildLightZClustersConstants)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<math::Vector2u>, rwClusterRanges)
END_SHADER_STRUCT();


DS_BEGIN(BuildLightZClustersDS, rg::RGDescriptorSetState<BuildLightZClustersDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<math::Vector2f>),             u_localLightsZRanges)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<LightsRenderingData>),          u_lightsData)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<BuildLightZClustersConstants>), u_constants)
DS_END();


BEGIN_SHADER_STRUCT(GenerateLightDrawCommandsConstants)
	SHADER_STRUCT_FIELD(Uint32, pointLightProxyVerticesNum)
	SHADER_STRUCT_FIELD(Uint32, spotLightProxyVerticesNum)
END_SHADER_STRUCT();


DS_BEGIN(GenerateLightsDrawCommnadsDS, rg::RGDescriptorSetState<GenerateLightsDrawCommnadsDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<LocalLightGPUData>),                  u_localLights)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<LightsRenderingData>),                  u_lightsData)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneViewCullingData>),                 u_sceneViewCullingData)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<GenerateLightDrawCommandsConstants>),   u_constants)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWStructuredBufferBinding<LightIndirectDrawCommand>), u_pointLightDraws)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWStructuredBufferBinding<Uint32>),                   u_pointLightDrawsCount)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWStructuredBufferBinding<LightIndirectDrawCommand>), u_spotLightDraws)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWStructuredBufferBinding<Uint32>),                   u_spotLightDrawsCount)
DS_END();


DS_BEGIN(BuildLightTilesDS, rg::RGDescriptorSetState<BuildLightTilesDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<LocalLightGPUData>),                u_localLights)
	DS_BINDING(BINDING_TYPE(gfx::OptionalStructuredBufferBinding<LightIndirectDrawCommand>), u_pointLightDraws)
	DS_BINDING(BINDING_TYPE(gfx::OptionalStructuredBufferBinding<LightIndirectDrawCommand>), u_spotLightDraws)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<math::Vector3f>),                   u_pointLightProxyVertices)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<math::Vector3f>),                   u_spotLightProxyVertices)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<LightsRenderingData>),                u_lightsData)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                         u_tilesLightsMask)
DS_END();



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

	const Real32 baseRadius = std::cos(angleStep * 0.5f);

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
		math::Vector3f(0.634456f, 0.634456f, 0.634456f), math::Vector3f(0.773488f, 0.000000f, 0.773488f), math::Vector3f(1.065260f, -0.000000f, 0.000000f), math::Vector3f(0.634456f, 0.634456f, 0.634456f), math::Vector3f(1.065260f, -0.000000f, 0.000000f),
		math::Vector3f(0.773488f, 0.773488f, -0.000000f), math::Vector3f(0.773488f, 0.000000f, 0.773488f), math::Vector3f(0.634456f, -0.634456f, 0.634456f), math::Vector3f(0.773488f, -0.773488f, 0.000000f), math::Vector3f(0.773488f, 0.000000f, 0.773488f),
		math::Vector3f(0.773488f, -0.773488f, 0.000000f), math::Vector3f(1.065260f, -0.000000f, 0.000000f), math::Vector3f(1.065260f, -0.000000f, 0.000000f), math::Vector3f(0.773488f, -0.773488f, 0.000000f), math::Vector3f(0.634456f, -0.634456f, -0.634456f),
		math::Vector3f(1.065260f, -0.000000f, 0.000000f), math::Vector3f(0.634456f, -0.634456f, -0.634456f), math::Vector3f(0.773489f, -0.000000f, -0.773489f), math::Vector3f(0.773488f, 0.773488f, -0.000000f), math::Vector3f(1.065260f, -0.000000f, 0.000000f),
		math::Vector3f(0.773489f, -0.000000f, -0.773489f), math::Vector3f(0.773488f, 0.773488f, -0.000000f), math::Vector3f(0.773489f, -0.000000f, -0.773489f), math::Vector3f(0.634456f, 0.634456f, -0.634456f), math::Vector3f(-0.634456f, 0.634456f, -0.634456f),
		math::Vector3f(0.000000f, 0.773489f, -0.773488f), math::Vector3f(0.000000f, 0.000000f, -1.065260f), math::Vector3f(-0.634456f, 0.634456f, -0.634456f), math::Vector3f(0.000000f, 0.000000f, -1.065260f), math::Vector3f(-0.773488f, -0.000000f, -0.773488f),
		math::Vector3f(0.000000f, 0.773489f, -0.773488f), math::Vector3f(0.634456f, 0.634456f, -0.634456f), math::Vector3f(0.773489f, -0.000000f, -0.773489f), math::Vector3f(0.000000f, 0.773489f, -0.773488f), math::Vector3f(0.773489f, -0.000000f, -0.773489f),
		math::Vector3f(0.000000f, 0.000000f, -1.065260f), math::Vector3f(0.000000f, 0.000000f, -1.065260f), math::Vector3f(0.773489f, -0.000000f, -0.773489f), math::Vector3f(0.634456f, -0.634456f, -0.634456f), math::Vector3f(0.000000f, 0.000000f, -1.065260f),
		math::Vector3f(0.634456f, -0.634456f, -0.634456f), math::Vector3f(-0.000000f, -0.773488f, -0.773489f), math::Vector3f(-0.773488f, -0.000000f, -0.773488f), math::Vector3f(0.000000f, 0.000000f, -1.065260f), math::Vector3f(-0.000000f, -0.773488f, -0.773489f),
		math::Vector3f(-0.773488f, -0.000000f, -0.773488f), math::Vector3f(-0.000000f, -0.773488f, -0.773489f), math::Vector3f(-0.634456f, -0.634456f, -0.634456f), math::Vector3f(-0.634456f, -0.634456f, -0.634456f), math::Vector3f(-0.000000f, -0.773488f, -0.773489f),
		math::Vector3f(0.000000f, -1.065260f, 0.000000f), math::Vector3f(-0.634456f, -0.634456f, -0.634456f), math::Vector3f(0.000000f, -1.065260f, 0.000000f), math::Vector3f(-0.773489f, -0.773488f, 0.000000f), math::Vector3f(-0.000000f, -0.773488f, -0.773489f),
		math::Vector3f(0.634456f, -0.634456f, -0.634456f), math::Vector3f(0.773488f, -0.773488f, 0.000000f), math::Vector3f(-0.000000f, -0.773488f, -0.773489f), math::Vector3f(0.773488f, -0.773488f, 0.000000f), math::Vector3f(0.000000f, -1.065260f, 0.000000f),
		math::Vector3f(0.000000f, -1.065260f, 0.000000f), math::Vector3f(0.773488f, -0.773488f, 0.000000f), math::Vector3f(0.634456f, -0.634456f, 0.634456f), math::Vector3f(0.000000f, -1.065260f, 0.000000f), math::Vector3f(0.634456f, -0.634456f, 0.634456f),
		math::Vector3f(-0.000000f, -0.773489f, 0.773488f), math::Vector3f(-0.773489f, -0.773488f, 0.000000f), math::Vector3f(0.000000f, -1.065260f, 0.000000f), math::Vector3f(-0.000000f, -0.773489f, 0.773488f), math::Vector3f(-0.773489f, -0.773488f, 0.000000f),
		math::Vector3f(-0.000000f, -0.773489f, 0.773488f), math::Vector3f(-0.634456f, -0.634456f, 0.634456f), math::Vector3f(-0.634456f, -0.634456f, 0.634456f), math::Vector3f(-0.773488f, 0.000000f, 0.773488f), math::Vector3f(-1.065260f, 0.000000f, -0.000000f),
		math::Vector3f(-0.634456f, -0.634456f, 0.634456f), math::Vector3f(-1.065260f, 0.000000f, -0.000000f), math::Vector3f(-0.773489f, -0.773488f, 0.000000f), math::Vector3f(-0.773488f, 0.000000f, 0.773488f), math::Vector3f(-0.634456f, 0.634457f, 0.634456f),
		math::Vector3f(-0.773488f, 0.773488f, -0.000000f), math::Vector3f(-0.773488f, 0.000000f, 0.773488f), math::Vector3f(-0.773488f, 0.773488f, -0.000000f), math::Vector3f(-1.065260f, 0.000000f, -0.000000f), math::Vector3f(-1.065260f, 0.000000f, -0.000000f),
		math::Vector3f(-0.773488f, 0.773488f, -0.000000f), math::Vector3f(-0.634456f, 0.634456f, -0.634456f), math::Vector3f(-1.065260f, 0.000000f, -0.000000f), math::Vector3f(-0.634456f, 0.634456f, -0.634456f), math::Vector3f(-0.773488f, -0.000000f, -0.773488f),
		math::Vector3f(-0.773489f, -0.773488f, 0.000000f), math::Vector3f(-1.065260f, 0.000000f, -0.000000f), math::Vector3f(-0.773488f, -0.000000f, -0.773488f), math::Vector3f(-0.773489f, -0.773488f, 0.000000f), math::Vector3f(-0.773488f, -0.000000f, -0.773488f),
		math::Vector3f(-0.634456f, -0.634456f, -0.634456f), math::Vector3f(-0.634456f, 0.634457f, 0.634456f), math::Vector3f(0.000000f, 0.773488f, 0.773489f), math::Vector3f(0.000000f, 1.065260f, 0.000000f), math::Vector3f(-0.634456f, 0.634457f, 0.634456f),
		math::Vector3f(0.000000f, 1.065260f, 0.000000f), math::Vector3f(-0.773488f, 0.773488f, -0.000000f), math::Vector3f(0.000000f, 0.773488f, 0.773489f), math::Vector3f(0.634456f, 0.634456f, 0.634456f), math::Vector3f(0.773488f, 0.773488f, -0.000000f),
		math::Vector3f(0.000000f, 0.773488f, 0.773489f), math::Vector3f(0.773488f, 0.773488f, -0.000000f), math::Vector3f(0.000000f, 1.065260f, 0.000000f), math::Vector3f(0.000000f, 1.065260f, 0.000000f), math::Vector3f(0.773488f, 0.773488f, -0.000000f),
		math::Vector3f(0.634456f, 0.634456f, -0.634456f), math::Vector3f(0.000000f, 1.065260f, 0.000000f), math::Vector3f(0.634456f, 0.634456f, -0.634456f), math::Vector3f(0.000000f, 0.773489f, -0.773488f), math::Vector3f(-0.773488f, 0.773488f, -0.000000f),
		math::Vector3f(0.000000f, 1.065260f, 0.000000f), math::Vector3f(0.000000f, 0.773489f, -0.773488f), math::Vector3f(-0.773488f, 0.773488f, -0.000000f), math::Vector3f(0.000000f, 0.773489f, -0.773488f), math::Vector3f(-0.634456f, 0.634456f, -0.634456f),
		math::Vector3f(-0.634456f, -0.634456f, 0.634456f), math::Vector3f(-0.000000f, -0.773489f, 0.773488f), math::Vector3f(0.000000f, 0.000000f, 1.065260f), math::Vector3f(-0.634456f, -0.634456f, 0.634456f), math::Vector3f(0.000000f, 0.000000f, 1.065260f),
		math::Vector3f(-0.773488f, 0.000000f, 0.773488f), math::Vector3f(-0.000000f, -0.773489f, 0.773488f), math::Vector3f(0.634456f, -0.634456f, 0.634456f), math::Vector3f(0.773488f, 0.000000f, 0.773488f), math::Vector3f(-0.000000f, -0.773489f, 0.773488f),
		math::Vector3f(0.773488f, 0.000000f, 0.773488f), math::Vector3f(0.000000f, 0.000000f, 1.065260f), math::Vector3f(0.000000f, 0.000000f, 1.065260f), math::Vector3f(0.773488f, 0.000000f, 0.773488f), math::Vector3f(0.634456f, 0.634456f, 0.634456f),
		math::Vector3f(0.000000f, 0.000000f, 1.065260f), math::Vector3f(0.634456f, 0.634456f, 0.634456f), math::Vector3f(0.000000f, 0.773488f, 0.773489f), math::Vector3f(-0.773488f, 0.000000f, 0.773488f), math::Vector3f(0.000000f, 0.000000f, 1.065260f),
		math::Vector3f(0.000000f, 0.773488f, 0.773489f), math::Vector3f(-0.773488f, 0.000000f, 0.773488f), math::Vector3f(0.000000f, 0.773488f, 0.773489f), math::Vector3f(-0.634456f, 0.634457f, 0.634456f)
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

	lib::MTHandle<BuildLightZClustersDS>		buildZClustersDS;
	lib::MTHandle<GenerateLightsDrawCommnadsDS>	generateLightsDrawCommnadsDS;
	lib::MTHandle<BuildLightTilesDS>			buildLightTilesDS;

	lib::MTHandle<ViewShadingInputDS> shadingInputDS;

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

static LightsInfo CreateLocalLightsData(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, rg::RGBufferViewHandle& OUT localLightsRGBuffer, rg::RGBufferViewHandle& OUT lightZRangesRGBuffer)
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

	const RenderSceneRegistry& sceneRegistry = renderScene.GetRegistry();

	const auto pointLightsView = sceneRegistry.view<const PointLightData>();
	const auto spotLightsView  = sceneRegistry.view<const SpotLightData>();

	const SizeType maxLocalLightsNum = pointLightsView.size() + spotLightsView.size();

	lib::DynamicArray<Real32> localLightsViewSpaceZ;
	localLights.reserve(maxLocalLightsNum);
	localLightsViewSpaceZ.reserve(maxLocalLightsNum);

	Uint32 pointLightsNum = 0u;

	for (const auto& [entity, pointLight] : pointLightsView.each())
	{
		LocalLightGPUData gpuLightData = GPUDataBuilder::CreatePointLightGPUData(pointLight);
		gpuLightData.entityID = static_cast<Uint32>(entity);
		gpuLightData.shadowMapFirstFaceIdx = idxNone<Uint32>;
		if (const PointLightShadowMapComponent* shadowMapComp = sceneRegistry.try_get<const PointLightShadowMapComponent>(entity))
		{
			gpuLightData.shadowMapFirstFaceIdx = shadowMapComp->shadowMapFirstFaceIdx;
			SPT_CHECK(gpuLightData.shadowMapFirstFaceIdx != idxNone<Uint32>);
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
	}

	Uint32 spotLightsNum = 0u;

	for (const auto& [entity, spotLight] : spotLightsView.each())
	{
		LocalLightGPUData gpuLightData = GPUDataBuilder::CreateSpotLightGPUData(spotLight);
		gpuLightData.entityID = static_cast<Uint32>(entity);
		gpuLightData.shadowMapFirstFaceIdx = idxNone<Uint32>;

		if (const PointLightShadowMapComponent* shadowMapComp = sceneRegistry.try_get<const PointLightShadowMapComponent>(entity))
		{
			gpuLightData.shadowMapFirstFaceIdx = shadowMapComp->shadowMapFirstFaceIdx;
			SPT_CHECK(gpuLightData.shadowMapFirstFaceIdx != idxNone<Uint32>);
		}

		const Real32 lightViewSpaceZ = getViewSpaceZ(gpuLightData);
		if (lightViewSpaceZ + gpuLightData.range > 0.f)
		{
			const auto emplaceIt = std::upper_bound(std::cbegin(localLightsViewSpaceZ), std::cend(localLightsViewSpaceZ), lightViewSpaceZ);
			const SizeType emplaceIdx = std::distance(std::cbegin(localLightsViewSpaceZ), emplaceIt);
			localLightsViewSpaceZ.emplace(emplaceIt, lightViewSpaceZ);
			localLights.emplace(std::cbegin(localLights) + emplaceIdx, gpuLightData);

			++spotLightsNum;
		}
	}

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

		const rhi::BufferDefinition lightZRangesBufferDefinition(localLightsZRanges.size() * sizeof(math::Vector2f), rhi::EBufferUsage::Storage);
		const lib::SharedRef<rdr::Buffer> lightZRangesBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("SceneLightZRanges"), lightZRangesBufferDefinition, rhi::EMemoryUsage::CPUToGPU);
		rdr::UploadDataToBuffer(lightZRangesBuffer, 0, reinterpret_cast<const Byte*>(localLightsZRanges.data()), localLightsZRanges.size() * sizeof(math::Vector2f));
		lightZRangesRGBuffer = graphBuilder.AcquireExternalBufferView(lightZRangesBuffer->GetFullView());
	}

	return LightsInfo
	{
		.pointLightsNum = pointLightsNum,
		.spotLightsNum  = spotLightsNum
	};
}

static Uint32 CreateDirectionalLightsData(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const lib::MTHandle<ViewShadingInputDS>& shadingInputDS)
{
	SPT_PROFILER_FUNCTION();

	const ViewDirectionalShadowMasksData* viewShadowMasks = viewSpec.GetBlackboard().Find<ViewDirectionalShadowMasksData>();

	const RenderSceneRegistry& sceneRegistry = renderScene.GetRegistry(); 

	const auto directionalLightsView = sceneRegistry.view<const DirectionalLightData, const DirectionalLightIlluminance>();
	const SizeType directionalLightsNum = sceneRegistry.view<const DirectionalLightData>().size();

	if (directionalLightsNum > 0)
	{
		lib::DynamicArray<rdr::HLSLStorage<DirectionalLightGPUData>> directionalLightsData;
		directionalLightsData.reserve(directionalLightsNum);

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

			directionalLightsData.emplace_back(lightGPUData);
		}

		const Uint64 directionalLightsBufferSize = directionalLightsData.size() * sizeof(rdr::HLSLStorage<DirectionalLightGPUData>);
		const rhi::BufferDefinition directionalLightsBufferDefinition(directionalLightsBufferSize, rhi::EBufferUsage::Storage);
		const lib::SharedRef<rdr::Buffer> directionalLightsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("View Directional Lights Buffer"), directionalLightsBufferDefinition, rhi::EMemoryUsage::CPUToGPU);
		rdr::UploadDataToBuffer(directionalLightsBuffer, 0, reinterpret_cast<const Byte*>(directionalLightsData.data()), directionalLightsBufferSize);

		shadingInputDS->u_directionalLights = directionalLightsBuffer->GetFullView();
	}

	return static_cast<Uint32>(directionalLightsNum);
}


struct LightsRenderingParams
{
	lib::SharedPtr<rdr::Buffer> pointLightProxyVerticesBuffer;
	lib::SharedPtr<rdr::Buffer> spotLightProxyVerticesBuffer;

	Uint32 pointLightProxyVerticesNum = 0u;
	Uint32 spotLightProxyVerticesNum = 0u;
};


static LightsRenderingDataPerView CreateLightsRenderingData(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const LightsRenderingParams& renderingParams)
{
	SPT_PROFILER_FUNCTION();

	const Uint32 zClustersNum = 32u;

	LightsRenderingDataPerView lightsRenderingDataPerView;

	LightsRenderingData lightsData;
	
	rg::RGBufferViewHandle localLightsBuffer;
	rg::RGBufferViewHandle localLightsZRangesBuffer;
	const LightsInfo lightsInfo = CreateLocalLightsData(graphBuilder, renderScene, viewSpec, OUT localLightsBuffer, OUT localLightsZRangesBuffer);
	const Uint32 localLightsNum = lightsInfo.pointLightsNum + lightsInfo.spotLightsNum;

	lightsRenderingDataPerView.spotLightsToRenderNum  = lightsInfo.spotLightsNum;
	lightsRenderingDataPerView.pointLightsToRenderNum = lightsInfo.pointLightsNum;

	const ParticipatingMediaViewRenderSystem& pmSystem = viewSpec.GetRenderView().GetRenderSystem<ParticipatingMediaViewRenderSystem>();

	const lib::MTHandle<ViewShadingInputDS> shadingInputDS = graphBuilder.CreateDescriptorSet<ViewShadingInputDS>(RENDERER_RESOURCE_NAME("ViewShadingInputDS"));

	lightsData.heightFog = pmSystem.GetHeightFogParams();

	if (lightsRenderingDataPerView.HasAnyLocalLightsToRender())
	{
		const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingRes();
		const math::Vector2u tilesNum = (renderingRes - math::Vector2u(1, 1)) / 8 + math::Vector2u(1, 1);
		const math::Vector2f tileSize = math::Vector2f(1.f / static_cast<Real32>(tilesNum.x()), 1.f / static_cast<Real32>(tilesNum.y()));

		lightsData.localLightsNum        = localLightsNum;
		lightsData.localLights32Num      = math::Utils::DivideCeil(localLightsNum, 32u);
		lightsData.zClusterLength        = params::localLightsZClustersLength;
		lightsData.zClustersNum          = zClustersNum;
		lightsData.tilesNum              = tilesNum;
		lightsData.tileSize              = tileSize;
		lightsData.ambientLightIntensity = params::ambientLightIntensity;

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
		buildClustersConstants.rwClusterRanges = clustersRanges;

		const lib::MTHandle<BuildLightZClustersDS> buildZClusters = graphBuilder.CreateDescriptorSet<BuildLightZClustersDS>(RENDERER_RESOURCE_NAME("BuildLightZClustersDS"));
		buildZClusters->u_localLightsZRanges = localLightsZRangesBuffer;
		buildZClusters->u_lightsData         = lightsData;
		buildZClusters->u_constants          = buildClustersConstants;

		const SceneViewCullingData& cullingData	= viewSpec.GetRenderView().GetCullingData();

		GenerateLightDrawCommandsConstants generateDrawCommandsConstants;
		generateDrawCommandsConstants.pointLightProxyVerticesNum = renderingParams.pointLightProxyVerticesNum;
		generateDrawCommandsConstants.spotLightProxyVerticesNum  = renderingParams.spotLightProxyVerticesNum;

		const lib::MTHandle<GenerateLightsDrawCommnadsDS> generateLightsDrawCommnadsDS = graphBuilder.CreateDescriptorSet<GenerateLightsDrawCommnadsDS>(RENDERER_RESOURCE_NAME("GenerateLightsDrawCommnadsDS"));
		generateLightsDrawCommnadsDS->u_localLights                 = localLightsBuffer;
		generateLightsDrawCommnadsDS->u_lightsData                  = lightsData;
		generateLightsDrawCommnadsDS->u_sceneViewCullingData        = cullingData;
		generateLightsDrawCommnadsDS->u_constants                   = generateDrawCommandsConstants;
		generateLightsDrawCommnadsDS->u_pointLightDraws             = pointLightDrawCommands;
		generateLightsDrawCommnadsDS->u_pointLightDrawsCount        = pointLightDrawCommandsCount;
		generateLightsDrawCommnadsDS->u_spotLightDraws              = spotLightDrawCommands;
		generateLightsDrawCommnadsDS->u_spotLightDrawsCount         = spotLightDrawCommandsCount;

		const lib::MTHandle<BuildLightTilesDS> buildLightTilesDS = graphBuilder.CreateDescriptorSet<BuildLightTilesDS>(RENDERER_RESOURCE_NAME("BuildLightTilesDS"));
		buildLightTilesDS->u_localLights              = localLightsBuffer;
		buildLightTilesDS->u_pointLightDraws          = pointLightDrawCommands;
		buildLightTilesDS->u_spotLightDraws           = spotLightDrawCommands;
		buildLightTilesDS->u_pointLightProxyVertices  = renderingParams.pointLightProxyVerticesBuffer->GetFullView();
		buildLightTilesDS->u_spotLightProxyVertices   = renderingParams.spotLightProxyVerticesBuffer->GetFullView();
		buildLightTilesDS->u_lightsData               = lightsData;
		buildLightTilesDS->u_tilesLightsMask          = tilesLightsMask;

		shadingInputDS->u_localLights     = localLightsBuffer;
		shadingInputDS->u_tilesLightsMask = tilesLightsMask;
		shadingInputDS->u_clustersRanges  = clustersRanges;

		lightsRenderingDataPerView.buildZClustersDS             = buildZClusters;
		lightsRenderingDataPerView.generateLightsDrawCommnadsDS = generateLightsDrawCommnadsDS;
		lightsRenderingDataPerView.buildLightTilesDS            = buildLightTilesDS;

		lightsRenderingDataPerView.pointLightDrawCommandsBuffer      = pointLightDrawCommands;
		lightsRenderingDataPerView.pointLightDrawCommandsCountBuffer = pointLightDrawCommandsCount;

		lightsRenderingDataPerView.spotLightDrawCommandsBuffer      = spotLightDrawCommands;
		lightsRenderingDataPerView.spotLightDrawCommandsCountBuffer = spotLightDrawCommandsCount;
	}

	const Uint32 directionalLightsNum = CreateDirectionalLightsData(graphBuilder, renderScene, viewSpec, INOUT shadingInputDS);
	lightsData.directionalLightsNum = directionalLightsNum;

	const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();
	shadingInputDS->u_ambientOcclusionTexture = viewContext.ambientOcclusion;

	const AtmosphereRenderSystem& atmosphereSystem = renderScene.GetRenderSystemChecked<AtmosphereRenderSystem>();
	const AtmosphereContext& atmosphereContext     = atmosphereSystem.GetAtmosphereContext();

	shadingInputDS->u_transmittanceLUT = graphBuilder.AcquireExternalTextureView(atmosphereContext.transmittanceLUT);
	shadingInputDS->u_atmosphereParams = atmosphereContext.atmosphereParamsBuffer->GetFullView();
	
	shadingInputDS->u_lightsData = lightsData;
	
	lightsRenderingDataPerView.shadingInputDS = shadingInputDS;

	lightsRenderingDataPerView.tilesNum		= lightsData.tilesNum;
	lightsRenderingDataPerView.zClustersNum	= lightsData.zClustersNum;

	return lightsRenderingDataPerView;
}

} // utils

//////////////////////////////////////////////////////////////////////////////////////////////////
// LightsRenderSystem ============================================================================

LightsRenderSystem::LightsRenderSystem(RenderScene& owningScene)
	: Super(owningScene)
{
	m_supportedStages = lib::Flags(ERenderStage::ForwardOpaque, ERenderStage::DeferredShading);
	
	m_globalLightsDS = rdr::ResourcesManager::CreateDescriptorSetState<GlobalLightsDS>(RENDERER_RESOURCE_NAME("Global Lights DS"));

	const lib::DynamicArray<math::Vector3f> spotLightProxyVertices = CreateSpotLightProxyVertices(12u);
	rhi::BufferDefinition spotLightProxyVerticesBufferDef(spotLightProxyVertices.size() * sizeof(math::Vector3f), lib::Flags(rhi::EBufferUsage::TransferDst, rhi::EBufferUsage::Storage));
	m_spotLightProxyVertices = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Spot Light Proxy Vertices"), spotLightProxyVerticesBufferDef, rhi::EMemoryUsage::GPUOnly);

	const lib::DynamicArray<math::Vector3f> pointLightProxyVertices = CreatePointLightProxyVertices();
	rhi::BufferDefinition pointLightProxyVerticesBufferDef(pointLightProxyVertices.size() * sizeof(math::Vector3f), lib::Flags(rhi::EBufferUsage::TransferDst, rhi::EBufferUsage::Storage));
	m_pointLightProxyVertices = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Point Light Proxy Vertices"), pointLightProxyVerticesBufferDef, rhi::EMemoryUsage::GPUOnly);

	rdr::UploadDataToBuffer(lib::Ref(m_spotLightProxyVertices), 0u, reinterpret_cast<const Byte*>(spotLightProxyVertices.data()), spotLightProxyVertices.size() * sizeof(math::Vector3f));
	rdr::UploadDataToBuffer(lib::Ref(m_pointLightProxyVertices), 0u, reinterpret_cast<const Byte*>(pointLightProxyVertices.data()), pointLightProxyVertices.size() * sizeof(math::Vector3f));
}

void LightsRenderSystem::CollectRenderViews(const RenderScene& renderScene, const RenderView& mainRenderView, INOUT RenderViewsCollector& viewsCollector)
{
	SPT_PROFILER_FUNCTION();

	Super::CollectRenderViews(renderScene, mainRenderView, viewsCollector);

	if (const lib::SharedPtr<ShadowMapsRenderSystem> shadowMapsRenderSystem = renderScene.FindRenderSystem<ShadowMapsRenderSystem>())
	{
		const lib::DynamicArray<RenderView*> shadowMapViewsToRender = shadowMapsRenderSystem->GetShadowMapViewsToUpdate();
		for (RenderView* renderView : shadowMapViewsToRender)
		{
			SPT_CHECK(!!renderView);
			viewsCollector.AddRenderView(*renderView);
		}
	}
}

void LightsRenderSystem::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings)
{
	Super::RenderPerFrame(graphBuilder, renderScene, viewSpecs, settings);

	for (ViewRenderingSpec* viewSpec : viewSpecs)
	{
		if (viewSpec->SupportsStage(ERenderStage::GlobalIllumination))
		{
			const rg::BindDescriptorSetsScope rendererDSScope(graphBuilder, rg::BindDescriptorSets(viewSpec->GetRenderView().GetRenderViewDS()));
			viewSpec->GetRenderStageEntries(ERenderStage::GlobalIllumination).GetPreRenderStage().AddRawMember(this, &LightsRenderSystem::CacheGlobalLightsDS);
		}

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

	utils::LightsRenderingParams lightRenderingParams;
	lightRenderingParams.pointLightProxyVerticesBuffer = m_pointLightProxyVertices;
	lightRenderingParams.spotLightProxyVerticesBuffer  = m_spotLightProxyVertices;
	lightRenderingParams.pointLightProxyVerticesNum    = static_cast<Uint32>(m_pointLightProxyVertices->GetSize() / sizeof(math::Vector3f));
	lightRenderingParams.spotLightProxyVerticesNum     = static_cast<Uint32>(m_spotLightProxyVertices->GetSize() / sizeof(math::Vector3f));
	
	const LightsRenderingDataPerView lightsRenderingData = utils::CreateLightsRenderingData(graphBuilder, renderScene, viewSpec, lightRenderingParams);

	if (lightsRenderingData.HasAnyLocalLightsToRender())
	{
		const math::Vector3u dispatchZClustersGroupsNum = math::Vector3u(lightsRenderingData.zClustersNum, 1, 1);
		graphBuilder.Dispatch(RG_DEBUG_NAME("BuildLightsZClusters"),
							  BuildLightZClustersPSO::pso,
							  dispatchZClustersGroupsNum,
							  rg::BindDescriptorSets(lightsRenderingData.buildZClustersDS));

		const math::Vector3u dispatchLightsGroupsNum = math::Vector3u(math::Utils::DivideCeil<Uint32>(lightsRenderingData.GetLocalLightsToRenderNum(), 32), 1, 1);
		graphBuilder.Dispatch(RG_DEBUG_NAME("GenerateLightsDrawCommands"),
							  GenerateLightsDrawCommandsPSO::pso,
							  dispatchLightsGroupsNum,
							  rg::BindDescriptorSets(lightsRenderingData.generateLightsDrawCommnadsDS, viewContext.depthCullingDS));

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
								rg::BindDescriptorSets(lightsRenderingData.buildLightTilesDS),
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

	viewContext.shadingInputDS = lightsRenderingData.shadingInputDS;

	RenderViewEntryDelegates::FillShadingDSData delegateParams;
	delegateParams.ds = viewContext.shadingInputDS;
	RenderViewEntryContext entryContext;
	entryContext.Bind<RenderViewEntryDelegates::FillShadingDSData>(std::move(delegateParams));
	viewSpec.GetRenderViewEntry(RenderViewEntryDelegates::FillShadingDS).Broadcast(graphBuilder, renderScene, viewSpec, entryContext);

	ViewSpecShadingParameters shadingParams;
	shadingParams.shadingInputDS = lightsRenderingData.shadingInputDS;
	viewSpec.GetBlackboard().Create<ViewSpecShadingParameters>(shadingParams);
}

void LightsRenderSystem::CacheGlobalLightsDS(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context)
{
	const RenderSceneRegistry& sceneRegistry = scene.GetRegistry();

	const ParticipatingMediaViewRenderSystem& pmSystem = viewSpec.GetRenderView().GetRenderSystem<ParticipatingMediaViewRenderSystem>();

	const auto pointLightsView = sceneRegistry.view<const PointLightData>();
	const auto spotLightsView  = sceneRegistry.view<const SpotLightData>();
	const SizeType localLightsNum = pointLightsView.size() + spotLightsView.size();

	lib::SharedPtr<rdr::Buffer> localLightsBuffer;
	if (localLightsNum > 0)
	{
		const Uint64 localLightsDataSize = localLightsNum * sizeof(rdr::HLSLStorage<LocalLightGPUData>);
		const rhi::BufferDefinition localLightsDataBufferDef(localLightsDataSize, rhi::EBufferUsage::Storage);
		localLightsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Local Lights Buffer"), localLightsDataBufferDef, rhi::EMemoryUsage::CPUToGPU);
		rhi::RHIMappedBuffer<rdr::HLSLStorage<LocalLightGPUData>> localLightsData(localLightsBuffer->GetRHI());

		SizeType localLightIdx = 0;

		for (const auto& [entity, pointLight] : pointLightsView.each())
		{
			LocalLightGPUData gpuLightData = GPUDataBuilder::CreatePointLightGPUData(pointLight);
			gpuLightData.entityID = static_cast<Uint32>(entity);
			gpuLightData.shadowMapFirstFaceIdx = idxNone<Uint32>;

			if (const PointLightShadowMapComponent* shadowMapComp = sceneRegistry.try_get<const PointLightShadowMapComponent>(entity))
			{
				gpuLightData.shadowMapFirstFaceIdx = shadowMapComp->shadowMapFirstFaceIdx;
				SPT_CHECK(gpuLightData.shadowMapFirstFaceIdx != idxNone<Uint32>);
			}

			localLightsData[localLightIdx++] = gpuLightData;
		}

		for (const auto& [entity, spotLight] : spotLightsView.each())
		{
			LocalLightGPUData gpuLightData = GPUDataBuilder::CreateSpotLightGPUData(spotLight);
			gpuLightData.entityID = static_cast<Uint32>(entity);
			gpuLightData.shadowMapFirstFaceIdx = idxNone<Uint32>;

			if (const PointLightShadowMapComponent* shadowMapComp = sceneRegistry.try_get<const PointLightShadowMapComponent>(entity))
			{
				gpuLightData.shadowMapFirstFaceIdx = shadowMapComp->shadowMapFirstFaceIdx;
				SPT_CHECK(gpuLightData.shadowMapFirstFaceIdx != idxNone<Uint32>);
			}

			localLightsData[localLightIdx++] = gpuLightData;
		}
	}

	const auto directionalLightsView = sceneRegistry.view<const DirectionalLightData, const DirectionalLightIlluminance>();
	const SizeType directionalLightsNum = sceneRegistry.view<const DirectionalLightData>().size();

	lib::SharedPtr<rdr::Buffer> directionalLightsBuffer;
	if (directionalLightsNum > 0)
	{
		const Uint64 directionalLightsDataSize = directionalLightsNum * sizeof(rdr::HLSLStorage<DirectionalLightGPUData>);
		const rhi::BufferDefinition directionalLightsDataBufferDef(directionalLightsDataSize, rhi::EBufferUsage::Storage);
		directionalLightsBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Global Directional Lights Buffer"), directionalLightsDataBufferDef, rhi::EMemoryUsage::CPUToGPU);
		rhi::RHIMappedBuffer<rdr::HLSLStorage<DirectionalLightGPUData>> directionalLightsData(directionalLightsBuffer->GetRHI());

		SizeType directionalLightIdx = 0;

		for (const auto& [entity, directionalLight, illuminance] : directionalLightsView.each())
		{
			SPT_CHECK(directionalLightIdx < directionalLightsData.GetElementsNum());

			DirectionalLightGPUData lightGPUData = GPUDataBuilder::CreateDirectionalLightGPUData(directionalLight, illuminance);
			lightGPUData.shadowMaskIdx = idxNone<Uint32>;

			directionalLightsData[directionalLightIdx++] = lightGPUData;
		}
	}

	GlobalLightsParams lightsParams;
	lightsParams.localLightsNum       = static_cast<Uint32>(localLightsNum);
	lightsParams.directionalLightsNum = static_cast<Uint32>(directionalLightsNum);
	lightsParams.heightFog            = pmSystem.GetHeightFogParams();

	lib::SharedPtr<AtmosphereRenderSystem> atmosphere = scene.FindRenderSystem<AtmosphereRenderSystem>();
	if (atmosphere && atmosphere->AreVolumetricCloudsEnabled())
	{
		const clouds::CloudsTransmittanceMap& cloudsTransmittanceMap = atmosphere->GetCloudsTransmittanceMap();

		lightsParams.hasValidCloudsTransmittanceMap = true;
		lightsParams.cloudsTransmittanceViewProj    = cloudsTransmittanceMap.viewProjectionMatrix;

		SPT_CHECK(!!cloudsTransmittanceMap.cloudsTransmittanceTexture);

		m_globalLightsDS->u_cloudsTransmittanceMap = cloudsTransmittanceMap.cloudsTransmittanceTexture;
	}
	else
	{
		lightsParams.hasValidCloudsTransmittanceMap = false;
		m_globalLightsDS->u_cloudsTransmittanceMap.Reset();
	}

	m_globalLightsDS->u_lightsParams       = lightsParams;
	m_globalLightsDS->u_localLights        = localLightsBuffer->GetFullView();
	m_globalLightsDS->u_directionalLights  = directionalLightsBuffer->GetFullView();
	m_globalLightsDS->u_brdfIntegrationLUT = BRDFIntegrationLUT::Get().GetLUT(graphBuilder);
}

} // spt::rsc
