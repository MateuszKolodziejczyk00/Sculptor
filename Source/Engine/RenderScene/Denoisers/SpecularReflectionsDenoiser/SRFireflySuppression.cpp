#include "SRFireflySuppression.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "ResourcesManager.h"
#include "View/RenderView.h"


namespace spt::rsc::sr_denoiser
{

BEGIN_SHADER_STRUCT(SRFireflySuppressionConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
END_SHADER_STRUCT();


DS_BEGIN(SRFireflySuppressionDS, rg::RGDescriptorSetState<SRFireflySuppressionDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<RTSphericalBasisType>),             u_outDiffuseY_SH2)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<RTSphericalBasisType>),             u_outSpecularY_SH2)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                   u_outDiffSpecCoCg)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<RTSphericalBasisType>),            u_inDiffuseY_SH2)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<RTSphericalBasisType>),            u_inSpecularY_SH2)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                  u_inDiffSpecCoCg)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                  u_normals)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SRFireflySuppressionConstants>), u_constants)
DS_END();


static rdr::PipelineStateID CreateFireflySuppressionPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/SpecularReflections/Denoiser/SRFireflySuppression.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "SRFireflySuppressionCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Specular Reflections Firefly Suppression Pipeline"), shader);
}


void SuppressFireflies(rg::RenderGraphBuilder& graphBuilder, const FireflySuppressionParams& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(params.normal.IsValid());
	SPT_CHECK(params.inSpecularY_SH2.IsValid());
	SPT_CHECK(params.inDiffuseY_SH2.IsValid());
	SPT_CHECK(params.inDiffSpecCoCg.IsValid());
	SPT_CHECK(params.outSpecularY_SH2.IsValid());
	SPT_CHECK(params.outDiffuseY_SH2.IsValid());
	SPT_CHECK(params.outDiffSpecCoCg.IsValid());

	const math::Vector2u resolution = params.inSpecularY_SH2->GetResolution2D();

	SRFireflySuppressionConstants shaderConstants;
	shaderConstants.resolution = resolution;

	lib::MTHandle<SRFireflySuppressionDS> ds = graphBuilder.CreateDescriptorSet<SRFireflySuppressionDS>(RENDERER_RESOURCE_NAME("SR Firefly Suppression DS"));
	ds->u_outDiffuseY_SH2  = params.outDiffuseY_SH2;
	ds->u_outSpecularY_SH2 = params.outSpecularY_SH2;
	ds->u_outDiffSpecCoCg  = params.outDiffSpecCoCg;
	ds->u_inDiffuseY_SH2   = params.inDiffuseY_SH2;
	ds->u_inSpecularY_SH2  = params.inSpecularY_SH2;
	ds->u_inDiffSpecCoCg   = params.inDiffSpecCoCg;
	ds->u_normals          = params.normal;
	ds->u_constants        = shaderConstants;

	static const rdr::PipelineStateID pipeline = CreateFireflySuppressionPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{} SR Firefly Suppression", params.debugName.AsString()),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds)));
								
}

} // spt::rsc::sr_denoiser
