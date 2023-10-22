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

	const DepthPrepassData& depthPrepassData = viewSpec.GetData().Get<DepthPrepassData>();
	SPT_CHECK(depthPrepassData.depth.IsValid());

	const RenderView& renderView = viewSpec.GetRenderView();

	const math::Vector3u renderingResolution = renderView.GetRenderingResolution3D();

	const rg::RGTextureViewHandle geometryNormalsTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("GeometryNormalsTexture"), rg::TextureDef(renderingResolution, rhi::EFragmentFormat::RGBA16_UN_Float));

	static rdr::PipelineStateID pipelineState = CompileGenerateGeometryNormalsPipeline();

	lib::SharedPtr<GenerateGeometryNormalsDS> generateGeometryNormalsDS = rdr::ResourcesManager::CreateDescriptorSetState<GenerateGeometryNormalsDS>(RENDERER_RESOURCE_NAME("GenerateGeometryNormalsDS"));
	generateGeometryNormalsDS->u_depthTexture			= depthPrepassData.depth;
	generateGeometryNormalsDS->u_geometryNormalsTexture	= geometryNormalsTexture;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Generate Geometry Normals"),
						  pipelineState,
						  math::Utils::DivideCeil(renderingResolution, math::Vector3u(8u, 8u, 1u)),
						  rg::BindDescriptorSets(std::move(generateGeometryNormalsDS), renderView.GetRenderViewDS()));

	ShadingInputData& shadingInputData = viewSpec.GetData().GetOrCreate<ShadingInputData>();
	shadingInputData.geometryNormals = geometryNormalsTexture;

	GetStageEntries(viewSpec).GetOnRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
