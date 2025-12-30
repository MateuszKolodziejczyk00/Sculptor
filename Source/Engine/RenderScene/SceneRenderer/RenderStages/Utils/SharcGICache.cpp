#include "SharcGICache.h"
#include "ResourcesManager.h"
#include "Types/Buffer.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "View/ViewRenderingSpec.h"
#include "RenderGraphBuilder.h"
#include "RayTracing/RayTracingRenderSceneSubsystem.h"
#include "MaterialsUnifiedData.h"
#include "GeometryManager.h"
#include "StaticMeshes/StaticMeshGeometry.h"
#include "RenderScene.h"
#include "Lights/LightsRenderSystem.h"
#include "DDGI/DDGISceneSubsystem.h"
#include "Atmosphere/Clouds/VolumetricCloudsTypes.h"
#include "Atmosphere/AtmosphereSceneSubsystem.h"
#include "FileSystem/File.h"
#include "Common/ShaderCompilationEnvironment.h"
#include "MaterialsSubsystem.h"


namespace spt::rsc
{

namespace sharc_passes
{

BEGIN_SHADER_STRUCT(SharcUpdateConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, invResolution)
	SHADER_STRUCT_FIELD(math::Vector3f, viewLocation)
	SHADER_STRUCT_FIELD(Uint32,         seed)
	SHADER_STRUCT_FIELD(math::Vector3f, prevViewLocation)
	SHADER_STRUCT_FIELD(Uint32,         frameIdx)
	SHADER_STRUCT_FIELD(Uint32,         sharcCapacity)
END_SHADER_STRUCT()


DS_BEGIN(SharcUpdateDS, rg::RGDescriptorSetState<SharcUpdateDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_skyViewLUT)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_transmittanceLUT)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<AtmosphereParams>),                    u_atmosphereParams)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                           u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                           u_baseColorMetallicTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                   u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint64>),                             u_hashEntries)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint64>),                             u_hashEntriesPrev)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<math::Vector4u>),                     u_voxelData)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<math::Vector4u>),                     u_voxelDataPrev)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SharcUpdateConstants>),                   u_constants)
DS_END();


DS_BEGIN(SharcResolveDS, rg::RGDescriptorSetState<SharcResolveDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint64>),                             u_hashEntries)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<math::Vector4u>),                     u_voxelData)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<math::Vector4u>),                     u_voxelDataPrev)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SharcUpdateConstants>),                   u_constants)
DS_END();


RT_PSO(SharcUpdatePSO)
{
	RAY_GEN_SHADER("Sculptor/SpecularReflections/SharcUpdate.hlsl", SharcUpdateRTG);

	MISS_SHADERS(
		SHADER_ENTRY("Sculptor/SpecularReflections/SharcUpdate.hlsl", GenericRTM)
	);

	HIT_GROUP
	{
		CLOSEST_HIT_SHADER("Sculptor/SpecularReflections/SharcUpdate.hlsl", GenericCHS);
		ANY_HIT_SHADER("Sculptor/SpecularReflections/SharcUpdate.hlsl",     GenericAH);

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


static rdr::PipelineStateID CreateSharcResolvePipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/SharcResolve.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "SharcResolveCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("SharcResolvePipeline"), shader);
}

} // sharc_passes

Bool SharcGICache::IsSharcSupported()
{
	static const Bool isSupported = std::filesystem::is_directory(lib::Path(sc::ShaderCompilationEnvironment::GetShadersPath()) / "Sharc");
	return isSupported;
}

SharcGICache::SharcGICache()
{
	constexpr Uint64 hashEntrySize = 8u;
	constexpr Uint64 voxelDataSize = 16u;

	rhi::BufferDefinition entriesBufferDef;
	entriesBufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst, rhi::EBufferUsage::TransferSrc);
	entriesBufferDef.size  = m_entriesNum * hashEntrySize;
	m_hashEntriesBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Hash Entries Buffer"), entriesBufferDef, rhi::EMemoryUsage::GPUOnly);

	rhi::BufferDefinition voxelDataDef;
	voxelDataDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
	voxelDataDef.size  = m_entriesNum * voxelDataSize;
	m_voxelData     = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Voxel Data Buffer (1)"), voxelDataDef, rhi::EMemoryUsage::GPUOnly);
	m_prevVoxelData = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Voxel Data Buffer (2)"), voxelDataDef, rhi::EMemoryUsage::GPUOnly);
}

void SharcGICache::Update(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "Update Sharc GI Cache");

	std::swap(m_voxelData, m_prevVoxelData);

	const RenderView& renderView = viewSpec.GetRenderView();

	ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	const rg::RGBufferViewHandle hashEntriesView = graphBuilder.AcquireExternalBufferView(m_hashEntriesBuffer->GetFullView());

	graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Clear Sharc voxel data"), graphBuilder.AcquireExternalBufferView(m_voxelData->GetFullView()), 0u);

	if (m_requresReset)
	{
		graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Clear Sharc prev voxel data"), graphBuilder.AcquireExternalBufferView(m_prevVoxelData->GetFullView()), 0u);
		graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Clear Sharc entries"), hashEntriesView, 0u);

		m_requresReset = false;
	}

	const math::Vector2u resolution = renderView.GetRenderingRes();

	sharc_passes::SharcUpdateConstants shaderConstants;
	shaderConstants.resolution       = resolution;
	shaderConstants.invResolution    = resolution.cast<Real32>().cwiseInverse();
	shaderConstants.viewLocation     = renderView.GetLocation();
	shaderConstants.seed             = renderView.GetRenderedFrameIdx(); // TODO
	shaderConstants.prevViewLocation = renderView.GetPrevFrameRenderingData().viewLocation;
	shaderConstants.frameIdx         = renderView.GetRenderedFrameIdx();
	shaderConstants.sharcCapacity    = m_entriesNum;

	const AtmosphereSceneSubsystem& atmosphereSubsystem = renderScene.GetSceneSubsystemChecked<AtmosphereSceneSubsystem>();
	const AtmosphereContext& atmosphereContext          = atmosphereSubsystem.GetAtmosphereContext();

	const math::Vector2u tracesNum = math::Utils::DivideCeil(resolution, math::Vector2u(5u, 5u));

	rhi::BufferDefinition hashEntriesBufferDef;
	hashEntriesBufferDef.size  = m_hashEntriesBuffer->GetSize();
	hashEntriesBufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
	const rg::RGBufferViewHandle hashEntriesPrev = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Sharc Hash Entries (Prev)"), hashEntriesBufferDef, rhi::EMemoryUsage::GPUOnly);

	graphBuilder.CopyFullBuffer(RG_DEBUG_NAME("Copy Prev Hash Entries"), hashEntriesView, hashEntriesPrev);

	lib::MTHandle<sharc_passes::SharcUpdateDS> updateDS = graphBuilder.CreateDescriptorSet<sharc_passes::SharcUpdateDS>(RENDERER_RESOURCE_NAME("Sharc Update DS"));
	updateDS->u_skyViewLUT               = viewContext.skyViewLUT;
	updateDS->u_transmittanceLUT         = atmosphereContext.transmittanceLUT;
	updateDS->u_atmosphereParams         = atmosphereContext.atmosphereParamsBuffer->GetFullView();
	updateDS->u_normalsTexture           = viewContext.octahedronNormals;
	updateDS->u_baseColorMetallicTexture = viewContext.gBuffer[GBuffer::Texture::BaseColorMetallic];
	updateDS->u_depthTexture             = viewContext.depth;
	updateDS->u_roughnessTexture         = viewContext.gBuffer[GBuffer::Texture::Roughness];
	updateDS->u_hashEntries              = m_hashEntriesBuffer->GetFullView();
	updateDS->u_hashEntriesPrev          = hashEntriesPrev;
	updateDS->u_voxelData                = m_voxelData->GetFullView();
	updateDS->u_voxelDataPrev            = m_prevVoxelData->GetFullView();
	updateDS->u_constants                = shaderConstants;

	LightsRenderSystem& lightsRenderSystem = renderScene.GetRenderSystemChecked<LightsRenderSystem>();
	const ddgi::DDGISceneSubsystem& ddgiSubsystem = renderScene.GetSceneSubsystemChecked<ddgi::DDGISceneSubsystem>();

	graphBuilder.TraceRays(RG_DEBUG_NAME("Update Sharc GI Cache"),
						   sharc_passes::SharcUpdatePSO::pso,
						   tracesNum,
						   rg::BindDescriptorSets(std::move(updateDS),
												  renderView.GetRenderViewDS(),
												  ddgiSubsystem.GetDDGISceneDS(),
												  lightsRenderSystem.GetGlobalLightsDS(),
												  viewContext.cloudscapeProbesDS));

	lib::MTHandle<sharc_passes::SharcResolveDS> resolveDS = graphBuilder.CreateDescriptorSet<sharc_passes::SharcResolveDS>(RENDERER_RESOURCE_NAME("Sharc Resolve DS"));
	resolveDS->u_hashEntries   = m_hashEntriesBuffer->GetFullView();
	resolveDS->u_voxelData     = m_voxelData->GetFullView();
	resolveDS->u_voxelDataPrev = m_prevVoxelData->GetFullView();
	resolveDS->u_constants     = shaderConstants;

	static rdr::PipelineStateID sharcResolvePipeline = sharc_passes::CreateSharcResolvePipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Resolve Sharc GI Cache"),
						  sharcResolvePipeline,
						  math::Utils::DivideCeil(m_entriesNum, 64u),
						  rg::BindDescriptorSets(std::move(resolveDS), renderView.GetRenderViewDS()));

	SharcCacheConstants sharcCacheConstants;
	sharcCacheConstants.entriesNum = m_entriesNum;

	lib::MTHandle<SharcCacheDS> sharcCacheDS = graphBuilder.CreateDescriptorSet<SharcCacheDS>(RENDERER_RESOURCE_NAME("Sharc Cache DS"));
	sharcCacheDS->u_hashEntries         = m_hashEntriesBuffer->GetFullView();
	sharcCacheDS->u_voxelData           = m_voxelData->GetFullView();
	sharcCacheDS->u_sharcCacheConstants = sharcCacheConstants;

	viewContext.sharcCacheDS = std::move(sharcCacheDS);
}

} // spt::rsc
