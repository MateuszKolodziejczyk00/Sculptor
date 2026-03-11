#include "SRClampHistory.h"
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

BEGIN_SHADER_STRUCT(SRClampHistoryConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, pixelSize)
END_SHADER_STRUCT();


DS_BEGIN(SRClampHistoryDS, rg::RGDescriptorSetState<SRClampHistoryDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<RTSphericalBasisType>),       u_specularY_SH2)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<RTSphericalBasisType>),       u_diffuseY_SH2)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),             u_diffSpecCoCg)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),            u_fastHistorySpecularTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),            u_fastHistoryDiffuseTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Uint32>),                     u_specularHistoryLengthTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Uint32>),                     u_diffuseHistoryLengthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                    u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),            u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                    u_roughnessTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                    u_diffuseHistoryLength)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                    u_specularHistoryLength)
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

	SPT_CHECK(params.fastHistorySpecularTexture.IsValid());
	SPT_CHECK(params.specularY_SH2.IsValid());
	SPT_CHECK(params.diffuseY_SH2.IsValid());
	SPT_CHECK(params.diffSpecCoCg.IsValid());
	SPT_CHECK(params.fastHistoryDiffuseTexture.IsValid());

	const math::Vector2u resolution = params.specularY_SH2->GetResolution2D();

	SRClampHistoryConstants shaderConstants;
	shaderConstants.resolution = resolution;
	shaderConstants.pixelSize  = resolution.cast<Real32>().cwiseInverse();

	lib::MTHandle<SRClampHistoryDS> ds = graphBuilder.CreateDescriptorSet<SRClampHistoryDS>(RENDERER_RESOURCE_NAME("SR Clamp History DS"));
	ds->u_specularY_SH2                = params.specularY_SH2;
	ds->u_diffuseY_SH2                 = params.diffuseY_SH2;
	ds->u_diffSpecCoCg                 = params.diffSpecCoCg;
	ds->u_fastHistorySpecularTexture   = params.fastHistorySpecularTexture;
	ds->u_fastHistoryDiffuseTexture    = params.fastHistoryDiffuseTexture;
	ds->u_specularHistoryLengthTexture = params.specularHistoryLengthTexture;
	ds->u_diffuseHistoryLengthTexture  = params.diffuseHistoryLengthTexture;
	ds->u_depthTexture                 = params.depthTexture;
	ds->u_normalsTexture               = params.normalsTexture;
	ds->u_roughnessTexture             = params.roughnessTexture;
	ds->u_diffuseHistoryLength         = params.diffuseHistoryLenght;
	ds->u_specularHistoryLength        = params.specularHistoryLenght;
	ds->u_constants                    = shaderConstants;

	static const rdr::PipelineStateID pipeline = CreateClampHistoryPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{} SR Clamp History", params.debugName.AsString()),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds)));
}

} // spt::rsc::sr_denoiser
