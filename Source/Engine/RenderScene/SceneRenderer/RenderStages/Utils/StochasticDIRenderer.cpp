#include "StochasticDIRenderer.h"
#include "RenderGraphBuilder.h"
#include "RenderScene.h"
#include "RenderSceneTypes.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "ShaderStructs.h"
#include "Bindless/BindlessTypes.h"
#include "Pipelines/PSOsLibraryTypes.h"
#include "Material.h"
#include "MaterialsSubsystem.h"
#include "StaticMeshes/RenderMesh.h"
#include "StaticMeshes/StaticMeshRenderingCommon.h"
#include "Transfers/UploadUtils.h"
#include "SceneRenderer/Utils/BRDFIntegrationLUT.h"


namespace spt::rsc::stochastic_di
{

namespace renderer_params
{
RendererIntParameter risSamplesNum("RIS Samples Num", { "Stochastic DI" }, 2, 1, 20);
RendererBoolParameter enableTemporalResampling("Enable Temporal Resampling", { "Stochastic DI" }, true);
RendererIntParameter spatialSamplesNum("Spatial Samples Num", { "Stochastic DI" }, 8, 1, 20);
RendererIntParameter spatialResamplingRange("Spatial Resampling Range", { "Stochastic DI" }, 32, 1, 64);
RendererBoolParameter enableSurfaceCheckDuringSpatialResampling("Enable Surface Check During Spatial Resampling", { "Stochastic DI" }, false);
} // renderer_params


BEGIN_SHADER_STRUCT(EmissiveElement)
	SHADER_STRUCT_FIELD(RenderEntityGPUPtr,      entityPtr)
	SHADER_STRUCT_FIELD(SubmeshGPUPtr,           submeshPtr)
	SHADER_STRUCT_FIELD(mat::MaterialDataHandle, materialDataHandle)
	SHADER_STRUCT_FIELD(Uint16,                  emissiveMatFeatureID)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(EmissivesSampler)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<EmissiveElement>, emissives)
	SHADER_STRUCT_FIELD(Uint32,                            emissivesNum)
END_SHADER_STRUCT();


static EmissivesSampler CreateEmissivesSampler(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene)
{
	SPT_PROFILER_FUNCTION();

	EmissivesSampler sampler;

	const auto meshesView = renderScene.GetRegistry().view<const StaticMeshInstanceRenderData, const EntityGPUDataHandle, const rsc::MaterialSlotsComponent>();

	lib::DynamicArray<rdr::HLSLStorage<EmissiveElement>> emissiveElements;

	for (const auto& [entity, staticMeshRenderData, entityGPUDataHandle, materialsSlots] : meshesView.each())
	{
		const RenderMesh* mesh = staticMeshRenderData.staticMesh.Get();
		SPT_CHECK(!!mesh);

		const lib::Span<const SubmeshRenderingDefinition> submeshes = mesh->GetSubmeshes();

		for (Uint32 idx = 0; idx < submeshes.size(); ++idx)
		{
			const ecs::EntityHandle material = materialsSlots.slots[idx];
			const mat::MaterialProxyComponent& materialProxy = material.get<mat::MaterialProxyComponent>();

			if (materialProxy.params.emissive)
			{
				EmissiveElement elem;
				elem.entityPtr            = entityGPUDataHandle.GetGPUDataPtr();
				elem.submeshPtr           = mesh->GetSubmeshesGPUPtr() + idx;
				elem.materialDataHandle   = materialProxy.GetMaterialDataHandle();
				elem.emissiveMatFeatureID = materialProxy.params.features.emissiveDataID;

				emissiveElements.emplace_back(elem);
			}
		}
	}

	if (!emissiveElements.empty())
	{
		rhi::BufferDefinition bufferDef;
		bufferDef.size = sizeof(EmissiveElement) * emissiveElements.size();
		bufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
		lib::SharedRef<rdr::Buffer> emissivesBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Stochastic DI Emissives"), bufferDef, rhi::EMemoryUsage::GPUOnly);
		gfx::UploadDataToBuffer(emissivesBuffer, 0u, reinterpret_cast<const Byte*>(emissiveElements.data()), emissiveElements.size() * sizeof(rdr::HLSLStorage<EmissiveElement>));

		sampler.emissives    = graphBuilder.AcquireExternalBufferView(emissivesBuffer->GetFullView());
		sampler.emissivesNum = static_cast<Uint32>(emissiveElements.size());
	}
	else
	{
		sampler.emissivesNum = 0u;
	}

	return sampler;
}

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
	SHADER_STRUCT_FIELD(EmissivesSampler,                         emissivesSampler)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<DIPackedReservoir>,      historyReservoirs)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<DIPackedReservoir>, outReservoirs)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,                historyDepth)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>,        historyNormal)
	SHADER_STRUCT_FIELD(math::Vector2u,                           reservoirsResolution)
	SHADER_STRUCT_FIELD(Uint32,                                   risSamplesNum)
	SHADER_STRUCT_FIELD(Bool,                                     enableTemporalResampling)
END_SHADER_STRUCT();


RT_PSO(DIInitialSamplingPSO)
{
	RAY_GEN_SHADER("Sculptor/Lights/StochasticDI/InitialSampling.hlsl", InitialSamplingRTG);

	MISS_SHADERS(SHADER_ENTRY("Sculptor/Lights/StochasticDI/InitialSampling.hlsl", GenericRTM));

	HIT_GROUP
	{
		ANY_HIT_SHADER("Sculptor/Lights/StochasticDI/InitialSampling.hlsl", GenericAH);

		HIT_PERMUTATION_DOMAIN(mat::RTHitGroupPermutation);
	};

	PRESET(pso);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		const rhi::RayTracingPipelineDefinition psoDefinition{ .maxRayRecursionDepth = 1u };
		pso = CompilePSO(compiler, psoDefinition, mat::MaterialsSubsystem::Get().GetRTHitGroups<HitGroup>());
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
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<DIPackedReservoir>,   inReservoirs)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<DIPackedReservoir>, outReservoirs)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector2f>,     brdfIntegrationLUT)
	SHADER_STRUCT_FIELD(math::Vector2u,                           reservoirsResolution)
	SHADER_STRUCT_FIELD(Uint32,                                   spatialSamplesNum)
	SHADER_STRUCT_FIELD(Uint32,                                   spatialResamplingRange)
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
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector2f>, brdfIntegrationLUT)
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

void Renderer::Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const StochasticDIParams& diParams)
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

	rg::RGBufferViewHandle reservoirsBuffer = CreateDIReservoirs(graphBuilder, reservoirsResolution);

	{
		StochasticDIInitialSamplingConstants shaderConstants;
		shaderConstants.motion                   = viewContext.motion;
		shaderConstants.gBuffer                  = viewContext.gBuffer.GetGPUGBuffer(viewContext.depth);
		shaderConstants.emissivesSampler         = CreateEmissivesSampler(graphBuilder, renderScene);
		shaderConstants.outReservoirs            = reservoirsBuffer;
		shaderConstants.reservoirsResolution     = reservoirsResolution;
		shaderConstants.risSamplesNum            = renderer_params::risSamplesNum;
		shaderConstants.enableTemporalResampling = renderer_params::enableTemporalResampling;

		if (canReuseHistory)
		{
			shaderConstants.historyReservoirs = historyReservoirs;
			shaderConstants.historyDepth      = viewContext.historyDepth;
			shaderConstants.historyNormal     = viewContext.historyOctahedronNormals;
		}

		graphBuilder.TraceRays(RG_DEBUG_NAME("DI Initial Sampling"),
							   DIInitialSamplingPSO::pso,
							   resolution,
							   rg::EmptyDescriptorSets(),
							   shaderConstants);
	}

	const rg::RGTextureViewHandle specularHitDist = graphBuilder.CreateTextureView(RG_DEBUG_NAME("DI Specular Hit Dist"), rg::TextureDef(resolution, rhi::EFragmentFormat::RGBA16_S_Float));
	const rg::RGTextureViewHandle diffuse         = graphBuilder.CreateTextureView(RG_DEBUG_NAME("DI Diffuse"), rg::TextureDef(resolution, rhi::EFragmentFormat::RGBA16_S_Float));
	const rg::RGTextureViewHandle lightDirection  = graphBuilder.CreateTextureView(RG_DEBUG_NAME("DI Light Direction"), rg::TextureDef(resolution, rhi::EFragmentFormat::RG16_UN_Float));

	const rg::RGTextureViewHandle brdfIntegrationLUT = BRDFIntegrationLUT::Get().GetLUT(graphBuilder);

	{
		DISpatialResamplingConstants shaderConstants;
		shaderConstants.rwSpecularHitDist      = specularHitDist;
		shaderConstants.rwDiffuse              = diffuse;
		shaderConstants.rwLightDirection       = lightDirection;
		shaderConstants.gBuffer                = viewContext.gBuffer.GetGPUGBuffer(viewContext.depth);
		shaderConstants.inReservoirs           = reservoirsBuffer;
		shaderConstants.outReservoirs          = historyReservoirs;
		shaderConstants.brdfIntegrationLUT     = brdfIntegrationLUT;
		shaderConstants.reservoirsResolution   = reservoirsResolution;
		shaderConstants.spatialSamplesNum      = renderer_params::spatialSamplesNum;
		shaderConstants.spatialResamplingRange = renderer_params::spatialResamplingRange;

		DISpatialResamplingPermutationDomain permutation;
		permutation.ENABLE_SURFACE_CHECK = renderer_params::enableSurfaceCheckDuringSpatialResampling;

		graphBuilder.TraceRays(RG_DEBUG_NAME("DI Spatial Resampling"),
							   DISpatialResamplingPSO::GetPermutation(permutation),
							   resolution,
							   rg::EmptyDescriptorSets(),
							   shaderConstants);
	}

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

	{
		ResolveStochasticDIConstants shaderConstants;
		shaderConstants.specular           = denoiserResult.denoisedSpecular;
		shaderConstants.diffuse            = denoiserResult.denoisedDiffuse;
		shaderConstants.lightDirection     = lightDirection;
		shaderConstants.rwLuminance        = diParams.outputLuminance;
		shaderConstants.brdfIntegrationLUT = brdfIntegrationLUT;
		shaderConstants.gBuffer            = viewContext.gBuffer.GetGPUGBuffer(viewContext.depth);

		graphBuilder.Dispatch(RG_DEBUG_NAME("Resolve Stochastic DI"),
							  ResolveStochasticDIPSO::pso,
							  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
							  rg::EmptyDescriptorSets(),
							  shaderConstants);
	}
}

} // spt::rsc::stochastic_di
