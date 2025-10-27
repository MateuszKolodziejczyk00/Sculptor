#include "SRDisocclussionFix.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "ResourcesManager.h"
#include "View/RenderView.h"
#include "SRDenoiserTypes.h"


namespace spt::rsc::sr_denoiser
{

BEGIN_SHADER_STRUCT(SRDisocclusionFixConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, pixelSize)
	SHADER_STRUCT_FIELD(Int32,          filterStride)
END_SHADER_STRUCT();


DS_BEGIN(SRDisocclusionFixDS, rg::RGDescriptorSetState<SRDisocclusionFixDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                       u_specularHistoryLengthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                       u_diffuseHistoryLengthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                       u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                       u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),               u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<RTSphericalBasisType>),         u_inSpecularY_SH2)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<RTSphericalBasisType>),         u_inDiffuseY_SH2)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),               u_inDiffSpecCoCg)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<RTSphericalBasisType>),          u_outSpecularY_SH2)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<RTSphericalBasisType>),          u_outDiffuseY_SH2)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                u_outDiffSpecCoCg)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SRDisocclusionFixConstants>), u_constants)
DS_END();


static rdr::PipelineStateID CreateDisocclusionFixPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/Denoiser/SRDisocclusionFix.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "SRDisocclusionFixCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Specular Reflections Disocclusion Fix Pipeline"), shader);
}


void DisocclusionFix(rg::RenderGraphBuilder& graphBuilder, const DisocclusionFixParams& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(params.inSpecularY_SH2.IsValid());
	SPT_CHECK(params.inDiffuseY_SH2.IsValid());
	SPT_CHECK(params.inDiffSpecCoCg.IsValid());
	SPT_CHECK(params.outSpecularY_SH2.IsValid());
	SPT_CHECK(params.outDiffuseY_SH2.IsValid());
	SPT_CHECK(params.outDiffSpecCoCg.IsValid());

	const math::Vector2u resolution = params.outDiffuseY_SH2->GetResolution2D();

	SRDisocclusionFixConstants shaderConstants;
	shaderConstants.resolution   = resolution;
	shaderConstants.pixelSize    = resolution.cast<Real32>().cwiseInverse();
	shaderConstants.filterStride = 4u;

	lib::MTHandle<SRDisocclusionFixDS> ds = graphBuilder.CreateDescriptorSet<SRDisocclusionFixDS>(RENDERER_RESOURCE_NAME("SR Disocclusion Fix DS"));
	ds->u_specularHistoryLengthTexture = params.specularHistoryLengthTexture;
	ds->u_diffuseHistoryLengthTexture  = params.diffuseHistoryLengthTexture;
	ds->u_depthTexture                 = params.depthTexture;
	ds->u_roughnessTexture             = params.roughnessTexture;
	ds->u_normalsTexture               = params.normalsTexture;
	ds->u_inSpecularY_SH2              = params.inSpecularY_SH2;
	ds->u_inDiffuseY_SH2               = params.inDiffuseY_SH2;
	ds->u_inDiffSpecCoCg               = params.inDiffSpecCoCg;
	ds->u_outSpecularY_SH2             = params.outSpecularY_SH2;
	ds->u_outDiffuseY_SH2              = params.outDiffuseY_SH2;
	ds->u_outDiffSpecCoCg              = params.outDiffSpecCoCg;
	ds->u_constants                    = shaderConstants;

	static const rdr::PipelineStateID pipeline = CreateDisocclusionFixPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{} SR Disocclusion Fix", params.debugName.AsString()),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds),
												 params.renderView.GetRenderViewDS()));
}

} // spt::rsc::sr_denoiser
