#include "SRATrousFilter.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "RGDescriptorSetState.h"
#include "ResourcesManager.h"
#include "RenderGraphBuilder.h"
#include "View/RenderView.h"


namespace spt::rsc::sr_denoiser
{

BEGIN_SHADER_STRUCT(SRATrousFilteringParams)
	SHADER_STRUCT_FIELD(Uint32, samplesOffset)
END_SHADER_STRUCT();


DS_BEGIN(SRATrousFilterDS, rg::RGDescriptorSetState<SRATrousFilterDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),            u_inputTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),             u_outputTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                     u_luminanceStdDevTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                    u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),            u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),            u_specularColorRoughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SRATrousFilteringParams>), u_params)
DS_END();


static rdr::PipelineStateID CreateSRATrousFilterPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/Denoiser/SRATrousFilter.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "SRATrousFilterCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("SR A-Trous Filter Pipeline"), shader);
}


void ApplyATrousFilter(rg::RenderGraphBuilder& graphBuilder, const SRATrousFilterParams& params, rg::RGTextureViewHandle input, rg::RGTextureViewHandle output, Uint32 iterationIdx)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector3u resolution = output->GetResolution();

	static const rdr::PipelineStateID pipeline = CreateSRATrousFilterPipeline();

	SRATrousFilteringParams dispatchParams;
	dispatchParams.samplesOffset = 1u << iterationIdx;

	lib::MTHandle<SRATrousFilterDS> ds = graphBuilder.CreateDescriptorSet<SRATrousFilterDS>(RENDERER_RESOURCE_NAME("SR A-Trous Filter DS"));
	ds->u_inputTexture           = input;
	ds->u_outputTexture          = output;
	ds->u_luminanceStdDevTexture = params.stdDevTexture;
	ds->u_depthTexture           = params.depthTexture;
	ds->u_normalsTexture         = params.normalsTexture;
	ds->u_specularColorRoughnessTexture = params.specularColorRoughnessTexture;
	ds->u_params                 = dispatchParams;

	graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("{}: Denoise Spatial A-Trous Filter (Iteration {})", params.name.Get().ToString(), iterationIdx)),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
						  rg::BindDescriptorSets(std::move(ds), params.renderView.GetRenderViewDS()));
}

} // spt::rsc::sr_denoiser
