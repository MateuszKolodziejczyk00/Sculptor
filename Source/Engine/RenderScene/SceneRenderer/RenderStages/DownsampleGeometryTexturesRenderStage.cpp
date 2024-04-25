#include "DownsampleGeometryTexturesRenderStage.h"
#include "RenderGraphBuilder.h"
#include "RGDescriptorSetState.h"
#include "Pipelines/PipelineState.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "ResourcesManager.h"


namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::DownsampleGeometryTextures, DownsampleGeometryTexturesRenderStage);


DS_BEGIN(DownsampleGeometryTexturesDS, rg::RGDescriptorSetState<DownsampleGeometryTexturesDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                            u_motionTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                            u_geometryNormalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>), u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                                     u_depthTextureHalfRes)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector2f>),                             u_motionTextureHalfRes)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),                             u_geometryNormalsTextureHalfRes)
DS_END();


static rdr::PipelineStateID CompileDownsampleGeometryTexturesPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/DownsampleGeometryTextures/DownsampleGeometryTextures.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "DownsampleGeometryTexturesCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("DownsampleGeometryTexturesPipeline"), shader);
}


DownsampleGeometryTexturesRenderStage::DownsampleGeometryTexturesRenderStage()
{ }

void DownsampleGeometryTexturesRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();
	SPT_CHECK(viewContext.depth.IsValid());
	SPT_CHECK(viewContext.motion.IsValid());
	SPT_CHECK(viewContext.geometryNormals.IsValid());

	const RenderView& renderView = viewSpec.GetRenderView();

	const math::Vector2u renderingResolution = renderView.GetRenderingRes();

	const math::Vector2u halfRes = math::Utils::DivideCeil(renderingResolution, math::Vector2u(2u, 2u));

	SPT_CHECK(viewContext.depthHalfRes.IsValid());
	SPT_CHECK(viewContext.depthHalfRes->GetResolution2D() == halfRes);

	const lib::MTHandle<DownsampleGeometryTexturesDS> ds = graphBuilder.CreateDescriptorSet<DownsampleGeometryTexturesDS>(RENDERER_RESOURCE_NAME("Downsample Geometry Textures DS"));
	ds->u_depthTexture                  = viewContext.depth;
	ds->u_motionTexture                 = viewContext.motion;
	ds->u_geometryNormalsTexture        = viewContext.geometryNormals;
	ds->u_depthTextureHalfRes           = viewContext.depthHalfRes;
	ds->u_motionTextureHalfRes          = viewContext.motionHalfRes;
	ds->u_geometryNormalsTextureHalfRes = viewContext.geometryNormalsHalfRes;

	const rdr::PipelineStateID pipeline = CompileDownsampleGeometryTexturesPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Downsample Geometry Textures"),
						  pipeline,
						  math::Utils::DivideCeil(halfRes, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(ds), renderView.GetRenderViewDS()));

	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
