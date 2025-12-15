#include "StochasticDIRenderer.h"
#include "RenderGraphBuilder.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ShaderStructs.h"
#include "Bindless/BindlessTypes.h"
#include "Pipelines/PSOsLibraryTypes.h"
#include "Material.h"
#include "MaterialsSubsystem.h"


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


BEGIN_SHADER_STRUCT(StochasticDIInitialSamplingConstants)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2DRef<math::Vector4f>, outputLuminance)
END_SHADER_STRUCT();


DS_BEGIN(StochasticDIInitialSamplingDS, rg::RGDescriptorSetState<StochasticDIInitialSamplingDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<StochasticDIInitialSamplingConstants>), u_constants)
DS_END();


void RenderDI(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const StochasticDIParams& diParams)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(diParams.outputLuminance.IsValid());
	
	const math::Vector2u resolution = diParams.outputLuminance->GetResolution2D();

	StochasticDIInitialSamplingConstants shaderConstants;
	shaderConstants.outputLuminance = diParams.outputLuminance;

	lib::MTHandle<StochasticDIInitialSamplingDS> ds = graphBuilder.CreateDescriptorSet<StochasticDIInitialSamplingDS>(RENDERER_RESOURCE_NAME("StochasticDIInitialSamplingDS"));
	ds->u_constants = shaderConstants;

	graphBuilder.TraceRays(RG_DEBUG_NAME("Initial Sampling"),
						   DIInitialSamplingPSO::pso,
						   resolution,
						   rg::BindDescriptorSets(std::move(ds)));
}

} // spt::rsc::stochastic_di
