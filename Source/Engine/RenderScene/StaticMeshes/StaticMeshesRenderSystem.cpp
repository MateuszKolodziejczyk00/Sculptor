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
#include "Common/ShaderCompilationInput.h"
#include "GeometryManager.h"

namespace spt::rsc
{

BEGIN_SHADER_STRUCT(, StaticMeshDrawCallData)
	SHADER_STRUCT_FIELD(Uint32, vertexCount)
	SHADER_STRUCT_FIELD(Uint32, instanceCount)
	SHADER_STRUCT_FIELD(Uint32, firstVertex)
	SHADER_STRUCT_FIELD(Uint32, firstInstance)
	SHADER_STRUCT_FIELD(Uint32, primitiveIdx)
END_SHADER_STRUCT();


BEGIN_RG_NODE_PARAMETERS_STRUCT(StaticMeshIndirectDataPerView)
	RG_BUFFER_VIEW(indirectBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
	RG_BUFFER_VIEW(indirectCountBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();


DS_BEGIN(, BuildIndirectStaticMeshCommandsDS, rg::RGDescriptorSetState<BuildIndirectStaticMeshCommandsDS>, rhi::EShaderStageFlags::Compute)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<StaticMeshDrawCallData>),	drawCommands)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),					drawCommandsCount)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<StaticMeshGPURenderData>),		staticMeshes)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<math::Matrix4f>),				instanceTransforms)
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

	sc::ShaderCompilationSettings compilationSettings;
	const rdr::ShaderID generateCommandsShader = rdr::ResourcesManager::CreateShader("StaticMeshes/GenerateIndirectCommands.hlsl", compilationSettings);
	indirectCommandsGenerationPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("GenerateIndirectCommandsPipeline"), generateCommandsShader);
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

	graphBuilder.AddDispatch(RG_DEBUG_NAME("Generate Static Mesh Indirect Command"),
							 indirectCommandsGenerationPipeline,
							 math::Vector3u(1, 1, 1),
							 rg::BindDescriptorSets(descriptorSetState, lib::Ref(GeometryManager::Get().GetPrimitivesDSState())));

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
