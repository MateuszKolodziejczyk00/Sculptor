#include "SharcGICache.h"
#include "Parameters/SceneRendererParams.h"
#include "SceneRenderSystems/Atmosphere/AtmosphereRenderSystem.h"
#include "ResourcesManager.h"
#include "Types/Buffer.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "Utils/ScreenSpaceTracer.h"
#include "Utils/ViewRenderingSpec.h"
#include "RenderGraphBuilder.h"
#include "RenderScene.h"
#include "SceneRenderSystems/Lights/LightsRenderSystem.h"
#include "FileSystem/File.h"
#include "Common/ShaderCompilationEnvironment.h"
#include "SceneRenderSystems/DDGi/DDGIRenderSystem.h"
#include "MaterialsSubsystem.h"


namespace spt::rsc
{

namespace renderer_params
{
RendererBoolParameter demodulateMaterials("Demodulate Materials", { "SHRAC" }, false);
RendererBoolParameter debugViewOn("Debug View On", { "SHRAC" }, false);
} // renderer_params

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
	SHADER_STRUCT_FIELD(SSTracerData,   ssrTracer)
	SHADER_STRUCT_FIELD(Real32,         ssrTraceLength)
	SHADER_STRUCT_FIELD(GPUGBuffer,     gpuGBuffer)
END_SHADER_STRUCT()


DS_BEGIN(SharcUpdateDS, rg::RGDescriptorSetState<SharcUpdateDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_skyViewLUT)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_transmittanceLUT)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<AtmosphereParams>),                    u_atmosphereParams)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_linearSampler)
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

	PERMUTATION_DOMAIN(SharcShadersPermutation);
	
	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		const rhi::RayTracingPipelineDefinition psoDefinition{ .maxRayRecursionDepth = 1u };
		CompilePermutation(compiler, psoDefinition, mat::MaterialsSubsystem::Get().GetRTHitGroups<HitGroup>(), SharcShadersPermutation{ .SHARC_DEMODULATE_MATERIALS = false });
		CompilePermutation(compiler, psoDefinition, mat::MaterialsSubsystem::Get().GetRTHitGroups<HitGroup>(), SharcShadersPermutation{ .SHARC_DEMODULATE_MATERIALS = true });
	}
};


COMPUTE_PSO(SharcResolvePSO)
{
	COMPUTE_SHADER("Sculptor/SpecularReflections/SharcResolve.hlsl", SharcResolveCS);

	PERMUTATION_DOMAIN(SharcShadersPermutation);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		CompilePermutation(compiler, SharcShadersPermutation{ .SHARC_DEMODULATE_MATERIALS = false });
		CompilePermutation(compiler, SharcShadersPermutation{ .SHARC_DEMODULATE_MATERIALS = true });
	}
};


BEGIN_SHADER_STRUCT(SharcDebugViewConstants)
	SHADER_STRUCT_FIELD(GPUGBuffer, gpuGBuffer)
END_SHADER_STRUCT()


BEGIN_SHADER_STRUCT(SharcDebugViewPermutation)
	SHADER_STRUCT_FIELD(rdr::DebugFeature,       sharcView)
	SHADER_STRUCT_FIELD(SharcShadersPermutation, sharcPermutation)
END_SHADER_STRUCT();


COMPUTE_PSO(SharcDebugViewPSO)
{
	COMPUTE_SHADER("Sculptor/SpecularReflections/SharcDebugView.hlsl", SharcDebugViewCS);

	PERMUTATION_DOMAIN(SharcDebugViewPermutation);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		CompilePermutation(compiler, SharcDebugViewPermutation{ .sharcView = rdr::DebugFeature(true), .sharcPermutation = SharcShadersPermutation{ .SHARC_DEMODULATE_MATERIALS = false } });
		CompilePermutation(compiler, SharcDebugViewPermutation{ .sharcView = rdr::DebugFeature(true), .sharcPermutation = SharcShadersPermutation{ .SHARC_DEMODULATE_MATERIALS = true } });
	}
};

} // sharc_passes

Bool SharcGICache::IsSharcSupported()
{
	static const Bool isSupported = std::filesystem::is_directory(lib::Path(sc::ShaderCompilationEnvironment::GetShadersPath()) / "Sharc");
	return isSupported;
}

Bool SharcGICache::IsDemodulatingMaterials()
{
	return renderer_params::demodulateMaterials;
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

void SharcGICache::Update(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SharcUpdateParams& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "Update Sharc GI Cache");

	if (IsDemodulatingMaterials() != m_isDemodulatingMaterials)
	{
		m_isDemodulatingMaterials = renderer_params::demodulateMaterials;
		m_requresReset = true;
	}

	std::swap(m_voxelData, m_prevVoxelData);

	const RenderView& renderView = viewSpec.GetRenderView();

	ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	const rg::RGBufferViewHandle hashEntriesView = graphBuilder.AcquireExternalBufferView(m_hashEntriesBuffer->GetFullView());

	graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Clear Sharc voxel data"), graphBuilder.AcquireExternalBufferView(m_voxelData->GetFullView()), 0u);

	if (m_requresReset || params.resetCache)
	{
		graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Clear Sharc prev voxel data"), graphBuilder.AcquireExternalBufferView(m_prevVoxelData->GetFullView()), 0u);
		graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Clear Sharc entries"), hashEntriesView, 0u);

		m_requresReset = false;
	}

	const math::Vector2u resolution = viewSpec.GetRenderingRes();

	sharc_passes::SharcUpdateConstants shaderConstants;
	shaderConstants.resolution       = resolution;
	shaderConstants.invResolution    = resolution.cast<Real32>().cwiseInverse();
	shaderConstants.viewLocation     = renderView.GetLocation();
	shaderConstants.seed             = viewSpec.GetFrameIdx(); // TODO
	shaderConstants.prevViewLocation = renderView.GetPrevFrameRenderingData().viewLocation;
	shaderConstants.frameIdx         = viewSpec.GetFrameIdx();
	shaderConstants.sharcCapacity    = m_entriesNum;
	shaderConstants.ssrTracer        = CreateScreenSpaceTracerData(viewContext.linearDepth, params.ssrStepsNum);
	shaderConstants.ssrTraceLength   = params.ssrTraceLength;
	shaderConstants.gpuGBuffer       = viewContext.gBuffer.GetGPUGBuffer(viewContext.depth);

	const AtmosphereRenderSystem& atmosphereSystem = rendererInterface.GetRenderSystemChecked<AtmosphereRenderSystem>();
	const AtmosphereContext& atmosphereContext     = atmosphereSystem.GetAtmosphereContext();

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
	updateDS->u_hashEntries              = m_hashEntriesBuffer->GetFullView();
	updateDS->u_hashEntriesPrev          = hashEntriesPrev;
	updateDS->u_voxelData                = m_voxelData->GetFullView();
	updateDS->u_voxelDataPrev            = m_prevVoxelData->GetFullView();
	updateDS->u_constants                = shaderConstants;

	const LightsRenderSystem& lightsRenderSystem   = rendererInterface.GetRenderSystemChecked<LightsRenderSystem>();
	const ddgi::DDGIRenderSystem& ddgiRenderSystem = rendererInterface.GetRenderSystemChecked<ddgi::DDGIRenderSystem>();

	const SharcShadersPermutation permutation = GetShadersPermutation();

	graphBuilder.TraceRays(RG_DEBUG_NAME("Update Sharc GI Cache"),
						   sharc_passes::SharcUpdatePSO::GetPermutation(permutation),
						   tracesNum,
						   rg::BindDescriptorSets(std::move(updateDS),
												  ddgiRenderSystem.GetDDGISceneDS(),
												  lightsRenderSystem.GetGlobalLightsDS(),
												  viewContext.cloudscapeProbesDS));

	lib::MTHandle<sharc_passes::SharcResolveDS> resolveDS = graphBuilder.CreateDescriptorSet<sharc_passes::SharcResolveDS>(RENDERER_RESOURCE_NAME("Sharc Resolve DS"));
	resolveDS->u_hashEntries   = m_hashEntriesBuffer->GetFullView();
	resolveDS->u_voxelData     = m_voxelData->GetFullView();
	resolveDS->u_voxelDataPrev = m_prevVoxelData->GetFullView();
	resolveDS->u_constants     = shaderConstants;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Resolve Sharc GI Cache"),
						  sharc_passes::SharcResolvePSO::GetPermutation(permutation),
						  math::Utils::DivideCeil(m_entriesNum, 64u),
						  rg::BindDescriptorSets(std::move(resolveDS)));

	SharcCacheConstants sharcCacheConstants;
	sharcCacheConstants.entriesNum = m_entriesNum;

	lib::MTHandle<SharcCacheDS> sharcCacheDS = graphBuilder.CreateDescriptorSet<SharcCacheDS>(RENDERER_RESOURCE_NAME("Sharc Cache DS"));
	sharcCacheDS->u_hashEntries         = m_hashEntriesBuffer->GetFullView();
	sharcCacheDS->u_voxelData           = m_voxelData->GetFullView();
	sharcCacheDS->u_sharcCacheConstants = sharcCacheConstants;

	viewContext.sharcCacheDS = std::move(sharcCacheDS);

	if (renderer_params::debugViewOn)
	{
		sharc_passes::SharcDebugViewConstants debugViewConstants;
		debugViewConstants.gpuGBuffer = viewContext.gBuffer.GetGPUGBuffer(viewContext.depth);

		sharc_passes::SharcDebugViewPermutation debugViewPermutation;
		debugViewPermutation.sharcView        = rdr::DebugFeature(true);
		debugViewPermutation.sharcPermutation = permutation;

		graphBuilder.Dispatch(RG_DEBUG_NAME("Sharc GI Cache Debug View"),
							  sharc_passes::SharcDebugViewPSO::GetPermutation(debugViewPermutation),
							  math::Utils::DivideCeil(resolution, math::Vector2u(16u, 16u)),
							  rg::BindDescriptorSets(viewContext.sharcCacheDS),
							  debugViewConstants);
	}
}

} // spt::rsc
