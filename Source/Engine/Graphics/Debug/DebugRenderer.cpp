#include "DebugRenderer.h"
#include "ResourcesManager.h"
#include "RenderGraphBuilder.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "RGDescriptorSetState.h"
#include "Pipelines/PSOsLibraryTypes.h"


namespace spt::gfx
{

BEGIN_SHADER_STRUCT(DebugDrawCallData)
	/* API Command data */
	SHADER_STRUCT_FIELD(Uint32, vertexCount)
	SHADER_STRUCT_FIELD(Uint32, instanceCount)
	SHADER_STRUCT_FIELD(Uint32, firstVertex)
	SHADER_STRUCT_FIELD(Uint32, firstInstance)
	
	/* Custom Data */
END_SHADER_STRUCT();


BEGIN_RG_NODE_PARAMETERS_STRUCT(DebugGeometryIndirectDrawCommandsParameters)
	RG_BUFFER_VIEW(linesDrawCall, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();


BEGIN_SHADER_STRUCT(DebugGeometryPassConstants)
	SHADER_STRUCT_FIELD(math::Matrix4f,            viewProjectionMatrix)
	SHADER_STRUCT_FIELD(TypedBufferRef<DebugLineDefinition>, lines)
END_SHADER_STRUCT();


DS_BEGIN(DebugGeometryPassDS, rg::RGDescriptorSetState<DebugGeometryPassDS>)
	DS_BINDING(BINDING_TYPE(ConstantBufferBinding<DebugGeometryPassConstants>), u_passConstants)
DS_END();


GRAPHICS_PSO(DebugGeometryPSO)
{
	VERTEX_SHADER("Sculptor/Debug/DebugGeometry.hlsl", DebugGeometryVS);
	FRAGMENT_SHADER("Sculptor/Debug/DebugGeometry.hlsl", DebugGeometryPS);

	PRESET(lines);

	static void PrecachePSOs(rdr::PSOCompilerInterface & compiler, const rdr::PSOPrecacheParams & params)
	{
		const rhi::PipelineRenderTargetsDefinition rtsDef
		{
			.colorRTsDefinition =
			{
				rhi::ColorRenderTargetDefinition
				{
					.format = rhi::EFragmentFormat::RGBA8_UN_Float,
				}
			},
			.depthRTDefinition =
			{
				.format = rhi::EFragmentFormat::D32_S_Float,
			}
		};

		lines = CompilePSO(compiler,
						   rhi::GraphicsPipelineDefinition
						   {
							   .primitiveTopology = rhi::EPrimitiveTopology::LineList,
							   .renderTargetsDefinition = rtsDef,
						   },
						   { "DEBUG_GEOMETRY_TYPE=0" });
	}
};


DebugRenderer::DebugRenderer(lib::HashedString name)
	: m_name(std::move(name))
{
	const rhi::BufferDefinition linesBufferDef(sizeof(rdr::HLSLStorage<DebugLineDefinition>) * 4096u, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst));
	m_gpuLines = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("GPU Debug Lines"), linesBufferDef, rhi::EMemoryUsage::GPUOnly);

	const rhi::BufferDefinition linesDrawCallBufferDef(sizeof(rdr::HLSLStorage<DebugDrawCallData>), lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst));
	m_gpuLinesDrawCall = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("GPU Debug Lines Draw Call"), linesDrawCallBufferDef, rhi::EMemoryUsage::GPUOnly);
	m_gpuLinesNumView  = m_gpuLinesDrawCall->CreateView(sizeof(Uint32), sizeof(Uint32));
}

void DebugRenderer::PrepareResourcesForRecording(rg::RenderGraphBuilder& graphBuilder, const DebugRenderingFrameSettings& frameSettings)
{
	SPT_PROFILER_FUNCTION();

	if (frameSettings.clearGeometry || !m_initialized)
	{
		graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Clear Debug Lines Num"), graphBuilder.AcquireExternalBufferView(m_gpuLinesDrawCall->GetFullView()), 0u);

		// Set vertex count
		graphBuilder.FillBuffer(RG_DEBUG_NAME("Clear Debug Lines Instances Count"), graphBuilder.AcquireExternalBufferView(m_gpuLinesDrawCall->GetFullView()), 0u, sizeof(Uint32), 2u);

		m_initialized = true;
	}
}

GPUDebugRendererData DebugRenderer::GetGPUDebugRendererData() const
{
	GPUDebugRendererData gpuData;
	gpuData.rwLines    = m_gpuLines->GetFullView();
	gpuData.rwLinesNum = m_gpuLinesNumView;
	return gpuData;
}

void DebugRenderer::RenderDebugGeometry(rg::RenderGraphBuilder& graphBuilder, const DebugRenderingSettings& settings)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(settings.outColor.IsValid());
	SPT_CHECK(settings.outDepth.IsValid());

	const math::Vector2u resolution = settings.outColor->GetResolution2D();
	SPT_CHECK(resolution == settings.outDepth->GetResolution2D());

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i::Zero(), resolution);

	rg::RGRenderTargetDef colorTargetDef;
	colorTargetDef.textureView    = settings.outColor;
	colorTargetDef.loadOperation  = rhi::ERTLoadOperation::Load;
	colorTargetDef.storeOperation = rhi::ERTStoreOperation::Store;
	colorTargetDef.clearColor     = rhi::ClearColor(0.f, 0.f, 0.f, 0.f);
	renderPassDef.AddColorRenderTarget(colorTargetDef);

	rg::RGRenderTargetDef depthTargetDef;
	depthTargetDef.textureView    = settings.outDepth;
	depthTargetDef.loadOperation  = rhi::ERTLoadOperation::Load;
	depthTargetDef.storeOperation = rhi::ERTStoreOperation::Store;
	renderPassDef.SetDepthRenderTarget(depthTargetDef);

	DebugGeometryIndirectDrawCommandsParameters indirectParams;
	indirectParams.linesDrawCall = graphBuilder.AcquireExternalBufferView(m_gpuLinesDrawCall->GetFullView());

	DebugGeometryPassConstants passConstants;
	passConstants.viewProjectionMatrix = settings.viewProjectionMatrix;
	passConstants.lines                = m_gpuLines->GetFullView();

	lib::MTHandle<DebugGeometryPassDS> ds = graphBuilder.CreateDescriptorSet<DebugGeometryPassDS>(RENDERER_RESOURCE_NAME("Debug Geometry Pass DS"));
	ds->u_passConstants = passConstants;

	graphBuilder.RenderPass(RG_DEBUG_NAME_FORMATTED("Render Debug Geometry: {}", m_name.GetData()),
							renderPassDef,
							rg::BindDescriptorSets(std::move(ds)),
							std::tie(indirectParams),
							[resolution, indirectParams](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), resolution.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), resolution));

								const rdr::BufferView& linesDrawCall = indirectParams.linesDrawCall->GetResourceRef();

								recorder.BindGraphicsPipeline(DebugGeometryPSO::lines);

								recorder.DrawIndirect(linesDrawCall, 0u, sizeof(rdr::HLSLStorage<DebugDrawCallData>), 1u);
							});

}

} // spt::gfx
