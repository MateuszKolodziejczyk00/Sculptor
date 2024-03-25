#include "MaterialDepthRenderer.h"
#include "RenderGraphBuilder.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ResourcesManager.h"
#include "Geometry/GeometryTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"


namespace spt::rsc
{

namespace material_depth_renderer
{

BEGIN_SHADER_STRUCT(MaterialDepthParams)
	SHADER_STRUCT_FIELD(math::Vector2f, screenResolution)
END_SHADER_STRUCT();


DS_BEGIN(CreateMaterialDepthDS, rg::RGDescriptorSetState<CreateMaterialDepthDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                u_visibilityTexture)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<GPUVisibleMeshlet>), u_visibleMeshlets)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<MaterialDepthParams>), u_materialDepthParams)
DS_END();


static rdr::PipelineStateID CompileMaterialDepthPipeline()
{
	const lib::String shadersPath = "Sculptor/GeometryRendering/MaterialDepth.hlsl";

	rdr::GraphicsPipelineShaders shaders;
	shaders.vertexShader   = rdr::ResourcesManager::CreateShader(shadersPath, sc::ShaderStageCompilationDef(rhi::EShaderStage::Vertex, "MaterialDepthVS"));
	shaders.fragmentShader = rdr::ResourcesManager::CreateShader(shadersPath, sc::ShaderStageCompilationDef(rhi::EShaderStage::Fragment, "MaterialDepthFS"));

	rhi::GraphicsPipelineDefinition pipelineDef;
	pipelineDef.renderTargetsDefinition.depthRTDefinition = rhi::DepthRenderTargetDefinition(constants::materialDepthFormat, rhi::ECompareOp::Always);

	return rdr::ResourcesManager::CreateGfxPipeline(RENDERER_RESOURCE_NAME("Create Material Depth Pipeline"), shaders, pipelineDef);
}

rg::RGTextureViewHandle CreateMaterialDepthTexture(rg::RenderGraphBuilder& graphBuilder, const math::Vector2u& resolution)
{
	return graphBuilder.CreateTextureView(RG_DEBUG_NAME("Material Depth"), rg::TextureDef(resolution, constants::materialDepthFormat));
}

void RenderMaterialDepth(rg::RenderGraphBuilder& graphBuilder, const MaterialDepthParameters& parameters, rg::RGTextureViewHandle materialDepth)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(parameters.visibilityTexture.IsValid());
	SPT_CHECK(materialDepth.IsValid());
	SPT_CHECK(parameters.visibleMeshlets.IsValid());
	SPT_CHECK(parameters.visibilityTexture->GetResolution() == materialDepth->GetResolution());

	const math::Vector2u resolution = materialDepth->GetResolution2D();

	static const rdr::PipelineStateID createMaterialDepthPipeline = CompileMaterialDepthPipeline();

	MaterialDepthParams materialDepthParams;
	materialDepthParams.screenResolution = resolution.cast<Real32>();

	lib::MTHandle<CreateMaterialDepthDS> materialDepthDS = graphBuilder.CreateDescriptorSet<CreateMaterialDepthDS>(RENDERER_RESOURCE_NAME("Create Material Depth DS"));
	materialDepthDS->u_visibilityTexture   = parameters.visibilityTexture;
	materialDepthDS->u_visibleMeshlets     = parameters.visibleMeshlets;
	materialDepthDS->u_materialDepthParams = materialDepthParams;

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView    = materialDepth;
	depthRTDef.loadOperation  = rhi::ERTLoadOperation::Clear;
	depthRTDef.storeOperation = rhi::ERTStoreOperation::Store;
	depthRTDef.clearColor     = rhi::ClearColor(1.f);

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i::Zero(), resolution);
	renderPassDef.SetDepthRenderTarget(depthRTDef);

	graphBuilder.RenderPass(RG_DEBUG_NAME("Render Material Depth"),
							renderPassDef,
							rg::BindDescriptorSets(std::move(materialDepthDS)),
							[resolution, pipeline = createMaterialDepthPipeline](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), resolution.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), resolution));

								recorder.BindGraphicsPipeline(pipeline);

								recorder.DrawInstances(3u, 1u);
							});
}

} // material_depth_renderer

} // spt::rsc
