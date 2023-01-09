#include "StaticMeshesRenderSystem.h"
#include "StaticMeshPrimitivesSystem.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RenderGraphBuilder.h"
#include "View/ViewRenderingSpec.h"
#include "RenderScene.h"

namespace spt::rsc
{

BEGIN_SHADER_STRUCT(, StaticMeshDrawCallData)
	SHADER_STRUCT_FIELD(Uint32, vertexCount)
	SHADER_STRUCT_FIELD(Uint32, instanceCount)
	SHADER_STRUCT_FIELD(Uint32, firstVertex)
	SHADER_STRUCT_FIELD(Uint32, firstInstance)
END_SHADER_STRUCT();


BEGIN_RG_NODE_PARAMETERS_STRUCT(StaticMeshIndirectDataPerView)
	RG_BUFFER_VIEW(indirectBuffer, rg::ERGBufferAccess::ShaderRead, rhi::EPipelineStage::DrawIndirect)
	RG_BUFFER_VIEW(indirectCountBuffer, rg::ERGBufferAccess::ShaderRead, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();


namespace utils
{

StaticMeshIndirectDataPerView CreateIndirectDataForView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene)
{
	SPT_PROFILER_FUNCTION();

	const rhi::BufferDefinition indirectBufferDef(sizeof(StaticMeshDrawCallData) * 1024, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect));
	const rhi::RHIAllocationInfo indirectBufferAllocation(rhi::EMemoryUsage::GPUOnly);

	StaticMeshIndirectDataPerView indirectData;
	indirectData.indirectBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("StaticMeshIndirectBuffer"), indirectBufferDef, indirectBufferAllocation);

	const rhi::BufferDefinition indirectCountBufferDef(sizeof(Uint32), rhi::EBufferUsage::Storage);
	const rhi::RHIAllocationInfo indirectCountBufferAllocation(rhi::EMemoryUsage::CPUToGPU);
	indirectData.indirectCountBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("StaticMeshIndirectCountBuffer"), indirectCountBufferDef, indirectCountBufferAllocation);
	SPT_CHECK_NO_ENTRY_MSG("indirectCountBuffer must be initialized to 0. TODO: figure out how to do it with render graph");

	return indirectData;
}

} // utils

StaticMeshesRenderSystem::StaticMeshesRenderSystem()
{
	m_supportedStages = lib::Flags(ERenderStage::GBufferGenerationStage, ERenderStage::ShadowGenerationStage);
}

void StaticMeshesRenderSystem::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& view)
{
	SPT_PROFILER_FUNCTION();

	Super::RenderPerView(graphBuilder, renderScene, view);

	/*const StaticMeshIndirectDataPerView& viewIndirectData =*/ view.GetData().Create<StaticMeshIndirectDataPerView>(utils::CreateIndirectDataForView(graphBuilder, renderScene));

	//SPT_CHECK_NO_ENTRY_MSG("TODO: Create shader");
	//rdr::PipelineStateID indirectCommandsGenerationPipeline;

	//graphBuilder.AddDispatch(RG_DEBUG_NAME("Generate Static Mesh Indirect Command"),
	//						 indirectCommandsGenerationPipeline,
	//						 math::Vector3u(1, 1, 1),
	//						 rg::BindDescriptorSets());

	if (view.SupportsStage(ERenderStage::GBufferGenerationStage))
	{
		RenderStageEntries& basePassStageEntries = view.GetRenderStageEntries(ERenderStage::GBufferGenerationStage);

		basePassStageEntries.GetOnRenderStage().AddRawMember(this, &StaticMeshesRenderSystem::RenderMeshesPerView);
	}
}

void StaticMeshesRenderSystem::RenderMeshesPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& view)
{
	SPT_PROFILER_FUNCTION();

	const StaticMeshIndirectDataPerView& indirectBuffers = view.GetData().Get<StaticMeshIndirectDataPerView>();

	rg::ForEachRGParameterAccess(indirectBuffers,
								 [](auto buffer, rg::ERGBufferAccess access, rhi::EPipelineStage pipelineStages)
								 {

								 });

	graphBuilder.AddSubpass(RG_DEBUG_NAME("Render Static Meshes"),
							rg::EmptyDescriptorSets(),
							std::tie(indirectBuffers),
							[&indirectBuffers](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{

							});
}

} // spt::rsc
