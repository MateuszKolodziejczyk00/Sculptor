#include "MaterialsRenderer.h"
#include "RenderGraphBuilder.h"
#include "View/ViewRenderingSpec.h"
#include "MaterialsUnifiedData.h"
#include "MaterialsSubsystem.h"
#include "StaticMeshes/StaticMeshGeometry.h"
#include "GeometryManager.h"


namespace spt::rsc
{

namespace materials_renderer
{

namespace priv
{

BEGIN_SHADER_STRUCT(MaterialBatchConstants)
	SHADER_STRUCT_FIELD(Real32, materialBatchDepth)
	SHADER_STRUCT_FIELD(Uint32, materialBatchIdx)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(EmitGBufferConstants)
	SHADER_STRUCT_FIELD(math::Vector2f, tileSizeNDC)
	SHADER_STRUCT_FIELD(math::Vector2f, screenResolution)
	SHADER_STRUCT_FIELD(math::Vector2f, invScreenResolution)
	SHADER_STRUCT_FIELD(Uint32,         groupsPerRow)
END_SHADER_STRUCT();


DS_BEGIN(MaterialBatchDS, rg::RGDescriptorSetState<MaterialBatchDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<MaterialBatchConstants>), u_materialBatchConstants)
DS_END();


DS_BEGIN(EmitGBufferDS, rg::RGDescriptorSetState<EmitGBufferDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<EmitGBufferConstants>), u_emitGBufferConstants)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2u>),         u_materialDepthTilesTexture)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<GPUVisibleMeshlet>),  u_visibleMeshlets)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                 u_visibilityTexture)
DS_END();


rdr::PipelineStateID CreateMaterialPipeline(const MaterialsPassParams& passParams, const MaterialBatch& materialBatch)
{
	SPT_PROFILER_FUNCTION();

	const ShadingViewContext& shadingContet = passParams.viewSpec.GetShadingViewContext();

	const mat::MaterialGraphicsShaders shaders = mat::MaterialsSubsystem::Get().GetMaterialShaders<mat::MaterialGraphicsShaders>("EmitGBuffer", materialBatch.materialShadersHash);

	rhi::GraphicsPipelineDefinition pipelineDef;
	pipelineDef.primitiveTopology = rhi::EPrimitiveTopology::TriangleList;
	pipelineDef.rasterizationDefinition.cullMode = rhi::ECullMode::None;
	pipelineDef.renderTargetsDefinition.depthRTDefinition = rhi::DepthRenderTargetDefinition(rhi::EFragmentFormat::D16_UN_Float, rhi::ECompareOp::Equal, false);
	std::transform(shadingContet.gBuffer.begin(), shadingContet.gBuffer.end(),
				   std::back_inserter(pipelineDef.renderTargetsDefinition.colorRTsDefinition),
				   [](const rg::RGTextureViewHandle& textureView)
				   {
					   return rhi::ColorRenderTargetDefinition(textureView->GetFormat(), rhi::ERenderTargetBlendType::Disabled);
				   });
	
	const rdr::PipelineStateID pipeline = rdr::ResourcesManager::CreateGfxPipeline(RENDERER_RESOURCE_NAME("Emit GBuffer Pipeline"),
																				   shaders.GetGraphicsPipelineShaders(),
																				   pipelineDef);

	return pipeline;
}


struct MaterialRenderCommand
{
	rdr::PipelineStateID           pipelineState;
	lib::MTHandle<MaterialBatchDS> materialBatchDS;
};


lib::DynamicArray<MaterialRenderCommand> CreateMaterialRenderCommands(rg::RenderGraphBuilder& graphBuilder, const MaterialsPassParams& passParams)
{
	SPT_PROFILER_FUNCTION();

	const lib::DynamicArray<MaterialBatch>& materialBatches = passParams.geometryPassData.materialBatches;

	lib::DynamicArray<MaterialRenderCommand> materialRenderCommands;
	materialRenderCommands.reserve(materialBatches.size());

	const Uint16 maxMaterialBatchIdx = std::numeric_limits<Uint16>::max();
	const Real32 materialDepthStep = 1.0f / maxMaterialBatchIdx;

	for (SizeType materialBatchIdx = 0u; materialBatchIdx < materialBatches.size(); ++materialBatchIdx)
	{
		SPT_CHECK(materialBatchIdx < std::numeric_limits<Uint16>::max());

		const Real32 materialBatchDepth = materialBatchIdx * materialDepthStep;

		MaterialBatchConstants materialBatchConstants;
		materialBatchConstants.materialBatchDepth = materialBatchDepth;
		materialBatchConstants.materialBatchIdx  = static_cast<Uint32>(materialBatchIdx);

		const lib::MTHandle<MaterialBatchDS> materialBatchDS = graphBuilder.CreateDescriptorSet<MaterialBatchDS>(RENDERER_RESOURCE_NAME("Material Batch DS"));
		materialBatchDS->u_materialBatchConstants = materialBatchConstants;

		MaterialRenderCommand& renderCommand = materialRenderCommands.emplace_back();
		renderCommand.pipelineState   = CreateMaterialPipeline(passParams, materialBatches[materialBatchIdx]);
		renderCommand.materialBatchDS = std::move(materialBatchDS);
	}

	return materialRenderCommands;
}


void RecordMaterialRenderCommands(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder, Uint32 groupsPerMaterial, lib::DynamicArray<MaterialRenderCommand> renderCommands)
{
	SPT_PROFILER_FUNCTION();

	for (const MaterialRenderCommand& renderCommand : renderCommands)
	{
		recorder.BindGraphicsPipeline(renderCommand.pipelineState);

		recorder.BindDescriptorSetState(renderCommand.materialBatchDS);

		recorder.DrawMeshTasks(math::Vector3u(groupsPerMaterial, 1u, 1u));

		recorder.UnbindDescriptorSetState(renderCommand.materialBatchDS);
	}
}


void ExecuteMaterialRenderCommands(rg::RenderGraphBuilder& graphBuilder, const MaterialsPassParams& passParams, lib::DynamicArray<MaterialRenderCommand> renderCommands)
{
	SPT_PROFILER_FUNCTION();

	const ViewRenderingSpec& viewSpec = passParams.viewSpec;
	const RenderView& renderView = viewSpec.GetRenderView();

	const ShadingViewContext& shadingContext = viewSpec.GetShadingViewContext();
	
	const math::Vector2u resolution = viewSpec.GetRenderingRes();

	const math::Vector2u tilesPerGroup = math::Vector2u(8u, 4u);
	const math::Vector2u pixelsPerGroup = tilesPerGroup * passParams.materialDepthTileSize;

	const math::Vector2u groupsPerMaterial2D = math::Utils::DivideCeil(resolution, pixelsPerGroup);
	const Uint32 groupsPerMaterial = groupsPerMaterial2D.x() * groupsPerMaterial2D.y();

	EmitGBufferConstants emitGBufferConstants;
	emitGBufferConstants.tileSizeNDC         = 2.f * math::Vector2f::Constant(static_cast<Real32>(passParams.materialDepthTileSize)).cwiseQuotient(resolution.cast<Real32>());
	emitGBufferConstants.screenResolution    = resolution.cast<Real32>();
	emitGBufferConstants.invScreenResolution = math::Vector2f::Ones().cwiseQuotient(emitGBufferConstants.screenResolution);
	emitGBufferConstants.groupsPerRow        = groupsPerMaterial2D.x();

	lib::MTHandle<EmitGBufferDS> emitGBufferDS = graphBuilder.CreateDescriptorSet<EmitGBufferDS>(RENDERER_RESOURCE_NAME("Emit GBuffer DS"));
	emitGBufferDS->u_emitGBufferConstants      = emitGBufferConstants;
	emitGBufferDS->u_materialDepthTilesTexture = passParams.materialDepthTileTexture;
	emitGBufferDS->u_visibleMeshlets           = passParams.visibleMeshletsBuffer;
	emitGBufferDS->u_visibilityTexture         = passParams.visibilityTexture;

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i::Zero(), resolution);

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView    = passParams.materialDepthTexture;
	depthRTDef.loadOperation  = rhi::ERTLoadOperation::Load;
	depthRTDef.storeOperation = rhi::ERTStoreOperation::Store;
	renderPassDef.SetDepthRenderTarget(depthRTDef);

	for (const rg::RGTextureViewHandle& gBufferTexture : shadingContext.gBuffer)
	{
		rg::RGRenderTargetDef gBufferRTDef;
		gBufferRTDef.textureView    = gBufferTexture;
		gBufferRTDef.loadOperation  = rhi::ERTLoadOperation::DontCare;
		gBufferRTDef.storeOperation = rhi::ERTStoreOperation::Store;
		renderPassDef.AddColorRenderTarget(gBufferRTDef);
	}

	graphBuilder.RenderPass(RG_DEBUG_NAME("Materials Pass"),
							renderPassDef,
							rg::BindDescriptorSets(mat::MaterialsUnifiedData::Get().GetMaterialsDS(),
												   StaticMeshUnifiedData::Get().GetUnifiedDataDS(),
												   renderView.GetRenderViewDS(),
												   GeometryManager::Get().GetGeometryDSState(),
												   emitGBufferDS),
							[resolution, groupsPerMaterial, commands = std::move(renderCommands)](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), resolution.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), resolution));

								RecordMaterialRenderCommands(renderContext, recorder, groupsPerMaterial, commands);
							});
}
} // priv

void ExecuteMaterialsPass(rg::RenderGraphBuilder& graphBuilder, const MaterialsPassParams& passParams)
{
	SPT_PROFILER_FUNCTION();

	const lib::DynamicArray<priv::MaterialRenderCommand> materialRenderCommands = priv::CreateMaterialRenderCommands(graphBuilder, passParams);

	priv::ExecuteMaterialRenderCommands(graphBuilder, passParams, materialRenderCommands);
}

} // materials_renderer

} // spt::rsc
