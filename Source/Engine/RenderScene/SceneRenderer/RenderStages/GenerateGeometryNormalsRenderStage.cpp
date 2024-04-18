#include "GenerateGeometryNormalsRenderStage.h"
#include "RenderGraphBuilder.h"
#include "RGDescriptorSetState.h"
#include "Pipelines/PipelineState.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "ResourcesManager.h"


namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::GenerateGeometryNormals, GenerateGeometryNormalsRenderStage);

DS_BEGIN(GenerateGeometryNormalsDS, rg::RGDescriptorSetState<GenerateGeometryNormalsDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),								u_geometryNormalsTexture)
DS_END();


static rdr::PipelineStateID CompileGenerateGeometryNormalsPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/GenerateGeometryNormals/GenerateGeometryNormals.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "GenerateGeometryNormalsCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("GenerateGeometryNormals"), shader);
}


GenerateGeometryNormalsRenderStage::GenerateGeometryNormalsRenderStage()
{ }

void GenerateGeometryNormalsRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();
	SPT_CHECK(viewContext.depth.IsValid());

	const RenderView& renderView = viewSpec.GetRenderView();

	const rhi::EFragmentFormat normalsFormat = rhi::EFragmentFormat::RGBA16_UN_Float;
	viewContext.geometryNormals        = graphBuilder.CreateTextureView(RG_DEBUG_NAME("GeometryNormalsTexture"), rg::TextureDef(renderView.GetRenderingRes(), normalsFormat));
	viewContext.geometryNormalsHalfRes = graphBuilder.CreateTextureView(RG_DEBUG_NAME("GeometryNormalsTextureHalfRes"), rg::TextureDef(renderView.GetRenderingHalfRes(), normalsFormat));

	static rdr::PipelineStateID pipelineState = CompileGenerateGeometryNormalsPipeline();

	lib::MTHandle<GenerateGeometryNormalsDS> generateGeometryNormalsDS = graphBuilder.CreateDescriptorSet<GenerateGeometryNormalsDS>(RENDERER_RESOURCE_NAME("GenerateGeometryNormalsDS"));
	generateGeometryNormalsDS->u_depthTexture			= viewContext.depth;
	generateGeometryNormalsDS->u_geometryNormalsTexture	= viewContext.geometryNormals;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Generate Geometry Normals"),
						  pipelineState,
						  math::Utils::DivideCeil(renderView.GetRenderingRes(), math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(generateGeometryNormalsDS), renderView.GetRenderViewDS()));

	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
