#include "StochasticDIRenderer.h"
#include "RenderGraphBuilder.h"
#include "RenderScene.h"
#include "RenderSceneTypes.h"
#include "ShaderStructs.h"
#include "Bindless/BindlessTypes.h"
#include "Pipelines/PSOsLibraryTypes.h"
#include "Material.h"
#include "MaterialsSubsystem.h"
#include "StaticMeshes/RenderMesh.h"
#include "Utils/ScreenSpaceTracer.h"
#include "Utils/TransfersUtils.h"
#include "Utils/SceneRenderingTypes.h"
#include "Utils/ViewRenderingSpec.h"
#include "ResourcesManager.h"


namespace spt::rsc::stochastic_di
{

BEGIN_SHADER_STRUCT(EmissiveElement)
	SHADER_STRUCT_FIELD(RenderEntityGPUPtr,      entityPtr)
	SHADER_STRUCT_FIELD(SubmeshGPUPtr,           submeshPtr)
	SHADER_STRUCT_FIELD(mat::MaterialDataHandle, materialDataHandle)
	SHADER_STRUCT_FIELD(Uint16,                  emissiveMatFeatureID)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(EmissivesSampler)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<EmissiveElement>, emissives)
	SHADER_STRUCT_FIELD(Uint32,                            emissivesNum)
	SHADER_STRUCT_FIELD(Uint32,                            emissivesNum2)
END_SHADER_STRUCT();


static EmissivesSampler CreateEmissivesSampler(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& scene)
{
	SPT_PROFILER_FUNCTION();

	EmissivesSampler sampler;

	lib::DynamicArenaArray<rdr::HLSLStorage<EmissiveElement>> emissiveElements(rendererInterface.GetSceneRendererFrameArena());

	const auto processMesh = [&](const RetainedDrawHandle handle, const RetainedDraw& draw)
	{
		const lib::Span<const SubmeshRenderingDefinition> submeshes = draw.mesh.GetSubmeshes();

		MaterialSlotsChunk* currentSlotsChunk = scene.materials.slots.Get(draw.materialSlots);
		Uint32 matSlotInChunkIdx = 0u;

		for (Uint32 idx = 0; idx < submeshes.size(); ++idx)
		{
			const ecs::EntityHandle material = currentSlotsChunk->slots[matSlotInChunkIdx++];
			const mat::MaterialProxyComponent& materialProxy = material.get<mat::MaterialProxyComponent>();

			if (materialProxy.params.emissive)
			{
				EmissiveElement elem;
				elem.entityPtr            = GetInstanceGPUDataPtr(draw.instance);
				elem.submeshPtr           = draw.mesh.GetSubmeshesGPUPtr() + idx;
				elem.materialDataHandle   = materialProxy.GetMaterialDataHandle();
				elem.emissiveMatFeatureID = materialProxy.params.features.emissiveDataID;

				emissiveElements.EmplaceBack(elem);
			}

			if (matSlotInChunkIdx >= currentSlotsChunk->slots.size())
			{
				currentSlotsChunk = scene.materials.slots.Get(currentSlotsChunk->next);
				matSlotInChunkIdx = 0u;
			}
		}
	};

	scene.draws.draws.ForEach(processMesh);

	if (!emissiveElements.empty())
	{
		rhi::BufferDefinition bufferDef;
		bufferDef.size = sizeof(EmissiveElement) * emissiveElements.size();
		bufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
		lib::SharedRef<rdr::Buffer> emissivesBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Stochastic DI Emissives"), bufferDef, rhi::EMemoryUsage::GPUOnly);
		rdr::UploadDataToBuffer(emissivesBuffer, 0u, reinterpret_cast<const Byte*>(emissiveElements.data()), emissiveElements.size() * sizeof(rdr::HLSLStorage<EmissiveElement>));

		sampler.emissives    = graphBuilder.AcquireExternalBufferView(emissivesBuffer->GetFullView());
		sampler.emissivesNum = static_cast<Uint32>(emissiveElements.size());
	}
	else
	{
		sampler.emissivesNum = 0u;
	}

	return sampler;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Presample Emissives ===========================================================================

static constexpr Uint32 presampledTilesNum = 128u;
static constexpr Uint32 presampledTileSize = 1024u;


BEGIN_SHADER_STRUCT(PackedEmissiveSample)
	SHADER_STRUCT_FIELD(math::Vector3f, location)
	SHADER_STRUCT_FIELD(Uint32,         packedLuminance)
	SHADER_STRUCT_FIELD(math::Vector2f, normal)
	SHADER_STRUCT_FIELD(Real32,         pdf_A)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(PresampleEmissivesConstants)
	SHADER_STRUCT_FIELD(EmissivesSampler,                            emissivesSampler)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<PackedEmissiveSample>, rwPresampledEmissives)
END_SHADER_STRUCT();


SIMPLE_COMPUTE_PSO(PresampleEmissivesPSO, "Sculptor/Lights/StochasticDI/PresampleEmissives.hlsl", PresampleEmissivesCS);

// TODO: this is temporary, reservoir should be "attached" to a sample, so it can detect sample that is not valid anymore. For now storing just location and normal is simpler because orded or emissive elements can change
BEGIN_SHADER_STRUCT(DIPackedReservoir)
	SHADER_STRUCT_FIELD(math::Vector3f, location)
	SHADER_STRUCT_FIELD(Uint32,         packedLuminance)
	SHADER_STRUCT_FIELD(math::Vector2f, hitNormal)
	SHADER_STRUCT_FIELD(Real32,         weight)
	SHADER_STRUCT_FIELD(Uint32,         MAndProps)
END_SHADER_STRUCT();

//////////////////////////////////////////////////////////////////////////////////////////////////
// Initial Sampling ==============================================================================

BEGIN_SHADER_STRUCT(StochasticDIInitialSamplingConstants)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector2f>,     motion)
	SHADER_STRUCT_FIELD(GPUGBuffer,                               gBuffer)
	SHADER_STRUCT_FIELD(SSTracerData,                             ssTracer)
	SHADER_STRUCT_FIELD(EmissivesSampler,                         emissivesSampler)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<PackedEmissiveSample>,    presampledEmissives)
	SHADER_STRUCT_FIELD(Uint32,                                   presampledTilesNum)
	SHADER_STRUCT_FIELD(Uint32,                                   presampledTileSize)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<DIPackedReservoir>,      historyReservoirs)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<DIPackedReservoir>, outReservoirs)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<DIPackedReservoir>, outInitialReservoirs)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,                historyDepth)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>,        historyNormal)
	SHADER_STRUCT_FIELD(math::Vector2u,                           reservoirsResolution)
	SHADER_STRUCT_FIELD(Uint32,                                   risSamplesNum)
	SHADER_STRUCT_FIELD(Bool,                                     enableTemporalResampling)
	SHADER_STRUCT_FIELD(Real32,                                   ssrtRange)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(DIInitialSamplingPermutationDomain)
	SHADER_STRUCT_FIELD(Bool, ENABLE_PRESAMPLING)
END_SHADER_STRUCT();


RT_PSO(DIInitialSamplingPSO)
{
	RAY_GEN_SHADER("Sculptor/Lights/StochasticDI/InitialSampling.hlsl", InitialSamplingRTG);

	MISS_SHADERS(SHADER_ENTRY("Sculptor/Lights/StochasticDI/InitialSampling.hlsl", GenericRTM));

	PERMUTATION_DOMAIN(DIInitialSamplingPermutationDomain);

	HIT_GROUP
	{
		ANY_HIT_SHADER("Sculptor/Lights/StochasticDI/InitialSampling.hlsl", GenericAH);

		HIT_PERMUTATION_DOMAIN(mat::RTHitGroupPermutation);
	};

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		const rhi::RayTracingPipelineDefinition psoDefinition{ .maxRayRecursionDepth = 1u };

		const lib::DynamicArray<HitGroup> hitGroups = mat::MaterialsSubsystem::Get().GetRTHitGroups<HitGroup>();

		CompilePermutation(compiler, psoDefinition, hitGroups, DIInitialSamplingPermutationDomain{ .ENABLE_PRESAMPLING = false });

		CompilePermutation(compiler, psoDefinition, hitGroups, DIInitialSamplingPermutationDomain{ .ENABLE_PRESAMPLING = true });
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// Spatial Resampling ============================================================================

BEGIN_SHADER_STRUCT(DISpatialResamplingPermutationDomain)
	SHADER_STRUCT_FIELD(Bool, ENABLE_SURFACE_CHECK)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(DISpatialResamplingConstants)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2DRef<math::Vector4f>,     rwSpecularHitDist)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2DRef<math::Vector3f>,     rwDiffuse)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2DRef<math::Vector2f>,     rwLightDirection)
	SHADER_STRUCT_FIELD(GPUGBuffer,                               gBuffer)
	SHADER_STRUCT_FIELD(SSTracerData,                             ssTracer)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<DIPackedReservoir>,   inReservoirs)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<DIPackedReservoir>,   initialReservoirs)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<DIPackedReservoir>, outReservoirs)
	SHADER_STRUCT_FIELD(math::Vector2u,                           reservoirsResolution)
	SHADER_STRUCT_FIELD(Uint32,                                   spatialSamplesNum)
	SHADER_STRUCT_FIELD(Uint32,                                   spatialResamplingRange)
	SHADER_STRUCT_FIELD(Real32,                                   ssrtRange)
END_SHADER_STRUCT();


RT_PSO(DISpatialResamplingPSO)
{
	RAY_GEN_SHADER("Sculptor/Lights/StochasticDI/DISpatialResampling.hlsl", DISpatialResamplingRTG);
	MISS_SHADERS(SHADER_ENTRY("Sculptor/Lights/StochasticDI/DISpatialResampling.hlsl", GenericRTM));

	PERMUTATION_DOMAIN(DISpatialResamplingPermutationDomain);

	HIT_GROUP
	{
		ANY_HIT_SHADER("Sculptor/Lights/StochasticDI/DISpatialResampling.hlsl", GenericAH);

		HIT_PERMUTATION_DOMAIN(mat::RTHitGroupPermutation);
	};

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		const rhi::RayTracingPipelineDefinition psoDefinition{ .maxRayRecursionDepth = 1u };

		const lib::DynamicArray<HitGroup> hitGroups = mat::MaterialsSubsystem::Get().GetRTHitGroups<HitGroup>();

		CompilePermutation(compiler, psoDefinition, hitGroups, DISpatialResamplingPermutationDomain{ .ENABLE_SURFACE_CHECK = false });

		CompilePermutation(compiler, psoDefinition, hitGroups, DISpatialResamplingPermutationDomain{ .ENABLE_SURFACE_CHECK = true });
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// Resolve DI ====================================================================================

SIMPLE_COMPUTE_PSO(ResolveStochasticDIPSO, "Sculptor/Lights/StochasticDI/ResolveStochasticDI.hlsl", ResolveStochasticDICS);


BEGIN_SHADER_STRUCT(ResolveStochasticDIConstants)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector3f>, specular)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector3f>, diffuse)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector2f>, lightDirection)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2DRef<math::Vector3f>, rwLuminance)
	SHADER_STRUCT_FIELD(GPUGBuffer,                           gBuffer)
END_SHADER_STRUCT();


math::Vector2u ComputeReservoirsResolution(math::Vector2u resolution)
{
	return math::Vector2u(math::Utils::RoundUp(resolution.x(), 64u), math::Utils::RoundUp(resolution.y(), 64u));
}

rg::RGBufferViewHandle CreateDIReservoirs(rg::RenderGraphBuilder& graphBuilder, math::Vector2u resolution)
{
	const math::Vector2u reservoirsResolution = ComputeReservoirsResolution(resolution);

	rhi::BufferDefinition bufferDef;
	bufferDef.size  = reservoirsResolution.x() * reservoirsResolution.y() * sizeof(DIPackedReservoir);
	bufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
	return graphBuilder.CreateBufferView(RG_DEBUG_NAME("DI Reservoirs Buffer"), bufferDef, rhi::EMemoryUsage::GPUOnly);
}


Renderer::Renderer()
	: m_denoiser(RG_DEBUG_NAME("Stochastic DI Denoiser"))
{
}

void Renderer::Render(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const StochasticDIParams& diParams)
{
	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "Stochastic DI");

	const math::Vector2u resolution = diParams.outputLuminance->GetResolution2D();

	Bool canReuseHistory = true;

	if (resolution != m_historyResolution || !m_historyReservoirs)
	{
		const math::Vector2u reservoirsResolution = ComputeReservoirsResolution(resolution);

		rhi::BufferDefinition bufferDef;
		bufferDef.size  = reservoirsResolution.x() * reservoirsResolution.y() * sizeof(DIPackedReservoir);
		bufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
		m_historyReservoirs = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Stochastic DI History Reservoirs"), bufferDef, rhi::EMemoryUsage::GPUOnly);

		m_historyResolution = resolution;
		canReuseHistory = false;
	}

	rg::RGBufferViewHandle historyReservoirs = graphBuilder.AcquireExternalBufferView(m_historyReservoirs->GetFullView());

	SPT_CHECK(diParams.outputLuminance.IsValid());

	const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	const math::Vector2u reservoirsResolution = ComputeReservoirsResolution(resolution);

	rg::RGBufferViewHandle reservoirsBuffer        = CreateDIReservoirs(graphBuilder, reservoirsResolution);
	rg::RGBufferViewHandle initialReservoirsBuffer = CreateDIReservoirs(graphBuilder, reservoirsResolution);

	const StochasticDISettings& settings = viewSpec.GetSettings().stochasticDI;

	const SSTracerData ssTracer = CreateScreenSpaceTracerData(viewContext.linearDepth, settings.ssrtSteps);

	const EmissivesSampler emissivesSampler = CreateEmissivesSampler(graphBuilder, rendererInterface, renderScene);

	rg::RGBufferViewHandle presampledEmissives;

	if (settings.enablePresampling)
	{
		rhi::BufferDefinition presampledEmissivesDef;
		presampledEmissivesDef.size  = presampledTilesNum * presampledTileSize * sizeof(rdr::HLSLStorage<PackedEmissiveSample>);
		presampledEmissivesDef.usage = rhi::EBufferUsage::Storage;
		presampledEmissives = graphBuilder.CreateBufferView(RG_DEBUG_NAME("DI Presampled Emissives"), presampledEmissivesDef, rhi::EMemoryUsage::GPUOnly);

		PresampleEmissivesConstants shaderConstants;
		shaderConstants.emissivesSampler      = emissivesSampler;
		shaderConstants.rwPresampledEmissives = presampledEmissives;

		graphBuilder.Dispatch(RG_DEBUG_NAME("DI Presample Emissives"),
							  PresampleEmissivesPSO::pso,
							  math::Vector3u(presampledTilesNum * presampledTileSize / 64u, 1u, 1u),
							  shaderConstants);
	}

	{
		StochasticDIInitialSamplingConstants shaderConstants;
		shaderConstants.motion                   = viewContext.motion;
		shaderConstants.gBuffer                  = viewContext.gBuffer.GetGPUGBuffer(viewContext.depth);
		shaderConstants.ssTracer                 = ssTracer;
		shaderConstants.emissivesSampler         = emissivesSampler;
		shaderConstants.presampledEmissives      = presampledEmissives;
		shaderConstants.presampledTilesNum       = presampledTilesNum;
		shaderConstants.presampledTileSize       = presampledTileSize;
		shaderConstants.outReservoirs            = reservoirsBuffer;
		shaderConstants.outInitialReservoirs     = initialReservoirsBuffer;
		shaderConstants.reservoirsResolution     = reservoirsResolution;
		shaderConstants.risSamplesNum            = settings.risSamples;
		shaderConstants.enableTemporalResampling = settings.enableTemporalResampling;
		shaderConstants.ssrtRange                = settings.ssrtRange;

		if (canReuseHistory)
		{
			shaderConstants.historyReservoirs = historyReservoirs;
			shaderConstants.historyDepth      = viewContext.historyDepth;
			shaderConstants.historyNormal     = viewContext.historyOctahedronNormals;
		}

		DIInitialSamplingPermutationDomain permutation;
		permutation.ENABLE_PRESAMPLING = settings.enablePresampling;

		graphBuilder.TraceRays(RG_DEBUG_NAME("DI Initial Sampling"),
							   DIInitialSamplingPSO::GetPermutation(permutation),
							   resolution,
							   rg::ShaderParams(shaderConstants));
	}

	const rg::RGTextureViewHandle specularHitDist = graphBuilder.CreateTextureView(RG_DEBUG_NAME("DI Specular Hit Dist"), rg::TextureDef(resolution, rhi::EFragmentFormat::RGBA16_S_Float));
	const rg::RGTextureViewHandle diffuse         = graphBuilder.CreateTextureView(RG_DEBUG_NAME("DI Diffuse"), rg::TextureDef(resolution, rhi::EFragmentFormat::RGBA16_S_Float));
	const rg::RGTextureViewHandle lightDirection  = graphBuilder.CreateTextureView(RG_DEBUG_NAME("DI Light Direction"), rg::TextureDef(resolution, rhi::EFragmentFormat::RG16_UN_Float));

	{
		DISpatialResamplingConstants shaderConstants;
		shaderConstants.rwSpecularHitDist      = specularHitDist;
		shaderConstants.rwDiffuse              = diffuse;
		shaderConstants.rwLightDirection       = lightDirection;
		shaderConstants.gBuffer                = viewContext.gBuffer.GetGPUGBuffer(viewContext.depth);
		shaderConstants.ssTracer               = ssTracer;
		shaderConstants.inReservoirs           = reservoirsBuffer;
		shaderConstants.initialReservoirs      = initialReservoirsBuffer;
		shaderConstants.outReservoirs          = historyReservoirs;
		shaderConstants.reservoirsResolution   = reservoirsResolution;
		shaderConstants.spatialSamplesNum      = settings.spatialSamples;
		shaderConstants.spatialResamplingRange = settings.spatialResamplingRange;
		shaderConstants.ssrtRange              = settings.ssrtRange;

		DISpatialResamplingPermutationDomain permutation;
		permutation.ENABLE_SURFACE_CHECK = settings.spatialResamplingSurfaceCheck;

		graphBuilder.TraceRays(RG_DEBUG_NAME("DI Spatial Resampling"),
							   DISpatialResamplingPSO::GetPermutation(permutation),
							   resolution,
							   rg::ShaderParams(shaderConstants));
	}

	rg::RGTextureViewHandle diffusePostDenoise  = diffuse;
	rg::RGTextureViewHandle specularPostDenoise = specularHitDist;

	const ShadingViewRenderingSystemsInfo& viewSystemsInfo = viewSpec.GetBlackboard().Get<ShadingViewRenderingSystemsInfo>();
	if (!viewSystemsInfo.useUnifiedDenoising)
	{
		sr_denoiser::Denoiser::Params denoiserParams(viewSpec);
		denoiserParams.currentDepthTexture      = viewContext.depth;
		denoiserParams.historyDepthTexture      = viewContext.historyDepth;
		denoiserParams.linearDepthTexture       = viewContext.linearDepth;
		denoiserParams.motionTexture            = viewContext.motion;
		denoiserParams.normalsTexture           = viewContext.octahedronNormals;
		denoiserParams.historyNormalsTexture    = viewContext.historyOctahedronNormals;
		denoiserParams.roughnessTexture         = viewContext.gBuffer[GBuffer::Texture::Roughness];
		denoiserParams.historyRoughnessTexture  = viewContext.historyRoughness;
		denoiserParams.specularTexture          = specularHitDist;
		denoiserParams.diffuseTexture           = diffuse;
		denoiserParams.baseColorMetallicTexture = viewContext.gBuffer[GBuffer::Texture::BaseColorMetallic];
		denoiserParams.lightDirection           = lightDirection;
		denoiserParams.resetAccumulation        = false;
		denoiserParams.blurVarianceEstimate     = false;
		const sr_denoiser::Denoiser::Result denoiserResult = m_denoiser.Denoise(graphBuilder, denoiserParams);

		diffusePostDenoise  = denoiserResult.denoisedDiffuse;
		specularPostDenoise = denoiserResult.denoisedSpecular;
	}
	else
	{
		viewSpec.GetShadingViewContext().specularHitDistance = specularHitDist;
	}

	{
		ResolveStochasticDIConstants shaderConstants;
		shaderConstants.specular       = specularPostDenoise;
		shaderConstants.diffuse        = diffusePostDenoise;
		shaderConstants.lightDirection = lightDirection;
		shaderConstants.rwLuminance    = diParams.outputLuminance;
		shaderConstants.gBuffer        = viewContext.gBuffer.GetGPUGBuffer(viewContext.depth);

		graphBuilder.Dispatch(RG_DEBUG_NAME("Resolve Stochastic DI"),
							  ResolveStochasticDIPSO::pso,
							  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
							  shaderConstants);
	}
}

} // spt::rsc::stochastic_di
