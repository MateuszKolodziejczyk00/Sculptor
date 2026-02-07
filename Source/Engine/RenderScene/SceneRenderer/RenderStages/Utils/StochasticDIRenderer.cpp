#include "StochasticDIRenderer.h"
#include "RenderGraphBuilder.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "RenderScene.h"
#include "RenderSceneTypes.h"
#include "ShaderStructs.h"
#include "Bindless/BindlessTypes.h"
#include "Pipelines/PSOsLibraryTypes.h"
#include "Material.h"
#include "MaterialsSubsystem.h"
#include "StaticMeshes/RenderMesh.h"
#include "StaticMeshes/StaticMeshRenderSceneSubsystem.h"
#include "Transfers/UploadUtils.h"


namespace spt::rsc::stochastic_di
{

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


BEGIN_SHADER_STRUCT(StochasticDIInitialSamplingConstants)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2DRef<math::Vector4f>, outputLuminance)
	SHADER_STRUCT_FIELD(GPUGBuffer,                           gBuffer)
	SHADER_STRUCT_FIELD(EmissivesSampler,                     emissivesSampler)
END_SHADER_STRUCT();


void RenderDI(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const StochasticDIParams& diParams)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(diParams.outputLuminance.IsValid());
	
	const math::Vector2u resolution = diParams.outputLuminance->GetResolution2D();

	const ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	StochasticDIInitialSamplingConstants shaderConstants;
	shaderConstants.outputLuminance  = diParams.outputLuminance;
	shaderConstants.gBuffer          = viewContext.gBuffer.GetGPUGBuffer(viewContext.depth);
	shaderConstants.emissivesSampler = CreateEmissivesSampler(graphBuilder, renderScene);

	graphBuilder.TraceRays(RG_DEBUG_NAME("Initial Sampling"),
						   DIInitialSamplingPSO::pso,
						   resolution,
						   rg::EmptyDescriptorSets(),
						   shaderConstants);
}

} // spt::rsc::stochastic_di
