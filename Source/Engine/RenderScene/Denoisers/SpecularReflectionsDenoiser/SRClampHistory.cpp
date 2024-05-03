#include "SRClampHistory.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "ResourcesManager.h"
#include "View/RenderView.h"


namespace spt::rsc::sr_denoiser
{

BEGIN_SHADER_STRUCT(SRClampHistoryConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, pixelSize)
END_SHADER_STRUCT();


DS_BEGIN(SRClampHistoryDS, rg::RGDescriptorSetState<SRClampHistoryDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),             u_luminanceAndHitDistanceTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Uint32>),                    u_accumulatedSamplesNumTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),            u_fastHistoryLuminanceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                    u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),            u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                    u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                    u_reprojectionConfidenceTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SRClampHistoryConstants>), u_constants)
DS_END();


static rdr::PipelineStateID CreateClampHistoryPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/Denoiser/SRClampHistory.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "SRClampHistoryCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Specular Reflections Clamp History Pipeline"), shader);
}


void ClampHistory(rg::RenderGraphBuilder& graphBuilder, const ClampHistoryParams& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.luminanceAndHitDistanceTexture->GetResolution2D();

	SRClampHistoryConstants shaderConstants;
	shaderConstants.resolution   = resolution;
	shaderConstants.pixelSize    = resolution.cast<Real32>().cwiseInverse();

	lib::MTHandle<SRClampHistoryDS> ds = graphBuilder.CreateDescriptorSet<SRClampHistoryDS>(RENDERER_RESOURCE_NAME("SR Clamp History DS"));
	ds->u_luminanceAndHitDistanceTexture = params.luminanceAndHitDistanceTexture;
	ds->u_fastHistoryLuminanceTexture    = params.fastHistoryLuminanceTexture;
	ds->u_accumulatedSamplesNumTexture   = params.accumulatedSamplesNumTexture;
	ds->u_depthTexture                   = params.depthTexture;
	ds->u_normalsTexture                 = params.normalsTexture;
	ds->u_roughnessTexture               = params.roughnessTexture;
	ds->u_reprojectionConfidenceTexture  = params.reprojectionConfidenceTexture;
	ds->u_constants                      = shaderConstants;

	static const rdr::PipelineStateID pipeline = CreateClampHistoryPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{} SR Clamp History", params.debugName.AsString()),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds),
												 params.renderView.GetRenderViewDS()));
}

} // spt::rsc::sr_denoiser
