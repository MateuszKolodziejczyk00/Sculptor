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
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, invResolution)
	SHADER_STRUCT_FIELD(Int32,          samplesOffset)
END_SHADER_STRUCT();


DS_BEGIN(SRATrousFilterDS, rg::RGDescriptorSetState<SRATrousFilterDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),            u_inputTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),             u_outputTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                     u_luminanceStdDevTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                    u_linearDepthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),            u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                    u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                    u_reprojectionConfidenceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                    u_historyFramesNumTexture)
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

	const math::Vector2u resolution = output->GetResolution2D();

	static const rdr::PipelineStateID pipeline = CreateSRATrousFilterPipeline();

	SRATrousFilteringParams dispatchParams;
	dispatchParams.samplesOffset = 1u << iterationIdx;
	dispatchParams.resolution    = resolution;
	dispatchParams.invResolution = resolution.cast<Real32>().cwiseInverse();

	lib::MTHandle<SRATrousFilterDS> ds = graphBuilder.CreateDescriptorSet<SRATrousFilterDS>(RENDERER_RESOURCE_NAME("SR A-Trous Filter DS"));
	ds->u_inputTexture                  = input;
	ds->u_outputTexture                 = output;
	ds->u_luminanceStdDevTexture        = params.stdDevTexture;
	ds->u_linearDepthTexture            = params.linearDepthTexture;
	ds->u_normalsTexture                = params.normalsTexture;
	ds->u_roughnessTexture              = params.roughnessTexture;
	ds->u_reprojectionConfidenceTexture = params.reprojectionConfidenceTexture;
	ds->u_historyFramesNumTexture       = params.historyFramesNumTexture;
	ds->u_params                        = dispatchParams;

	graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("{}: Denoise Spatial A-Trous Filter (Iteration {})", params.name.Get().ToString(), iterationIdx)),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 4u)),
						  rg::BindDescriptorSets(std::move(ds), params.renderView.GetRenderViewDS()));
}

} // spt::rsc::sr_denoiser
