#include "StaticMeshesRenderSystem.h"
#include "StaticMeshPrimitivesSystem.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RenderGraphBuilder.h"
#include "View/ViewRenderingSpec.h"
#include "RenderScene.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"

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


DS_BEGIN(, BuildIndirectStaticMeshCommandsDS, rg::RGDescriptorSetState<BuildIndirectStaticMeshCommandsDS>, rhi::EShaderStageFlags::Compute)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<StaticMeshDrawCallData, rg::ERGBufferAccess::ShaderWrite>),	drawCommands)
	DS_BINDING(BINDING_TYPE(gfx::RWBufferBinding<Uint32, rg::ERGBufferAccess::ShaderWrite>),							drawCommandsCount)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<StaticMeshGPURenderData, rg::ERGBufferAccess::ShaderRead>),	staticMeshes)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<math::Matrix4f, rg::ERGBufferAccess::ShaderRead>),			instanceTransforms)
DS_END();


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

	graphBuilder.AddFillBuffer(RG_DEBUG_NAME("Initialize Mesh Instances Count"), indirectData.indirectCountBuffer, 0, sizeof(Uint32), 0);

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

	const StaticMeshPrimitivesSystem& staticMeshPrimsSystem = renderScene.GetPrimitivesSystemChecked<StaticMeshPrimitivesSystem>();
	const lib::SharedRef<rdr::Buffer>& instancesBuffer = staticMeshPrimsSystem.GetStaticMeshInstances().GetBuffer();

	const StaticMeshIndirectDataPerView& viewIndirectData = view.GetData().Create<StaticMeshIndirectDataPerView>(utils::CreateIndirectDataForView(graphBuilder, renderScene));

	lib::SharedRef<BuildIndirectStaticMeshCommandsDS> descriptorSetState = rdr::ResourcesManager::CreateDescriptorSetState<BuildIndirectStaticMeshCommandsDS>(RENDERER_RESOURCE_NAME("IndirectCommandsDS"));
	descriptorSetState->drawCommands = viewIndirectData.indirectBuffer;
	descriptorSetState->drawCommandsCount = viewIndirectData.indirectCountBuffer;
	descriptorSetState->staticMeshes = instancesBuffer->CreateFullView();
	descriptorSetState->instanceTransforms = renderScene.GetTransformsBuffer()->CreateFullView();

	SPT_CHECK_NO_ENTRY_MSG("TODO: Create shader");
	rdr::PipelineStateID indirectCommandsGenerationPipeline;

	graphBuilder.AddDispatch(RG_DEBUG_NAME("Generate Static Mesh Indirect Command"),
							 indirectCommandsGenerationPipeline,
							 math::Vector3u(1, 1, 1),
							 rg::BindDescriptorSets(descriptorSetState));

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

	graphBuilder.AddSubpass(RG_DEBUG_NAME("Render Static Meshes"),
							rg::EmptyDescriptorSets(),
							std::tie(indirectBuffers),
							[&indirectBuffers](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{

							});
}

} // spt::rsc
