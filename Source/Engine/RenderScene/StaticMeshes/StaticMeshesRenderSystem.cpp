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
#include "View/RenderView.h"

namespace spt::rsc
{

BEGIN_SHADER_STRUCT(, StaticMeshDrawCallData)
	SHADER_STRUCT_FIELD(Uint32, vertexCount)
	SHADER_STRUCT_FIELD(Uint32, instanceCount)
	SHADER_STRUCT_FIELD(Uint32, firstVertex)
	SHADER_STRUCT_FIELD(Uint32, firstInstance)
	SHADER_STRUCT_FIELD(Uint32, primitiveIdx)
	SHADER_STRUCT_FIELD(Uint32, transformIdx)
END_SHADER_STRUCT();


BEGIN_RG_NODE_PARAMETERS_STRUCT(StaticMeshIndirectDataPerView)
	RG_BUFFER_VIEW(drawsBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
	RG_BUFFER_VIEW(countBuffer, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();


DS_BEGIN(, BuildIndirectStaticMeshCommandsDS, rg::RGDescriptorSetState<BuildIndirectStaticMeshCommandsDS>, rhi::EShaderStageFlags::Compute)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<StaticMeshDrawCallData>),	drawCommands)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),					drawCommandsCount)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<StaticMeshGPURenderData>),		staticMeshes)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<math::Matrix4f>),				instanceTransforms)
DS_END();


DS_BEGIN(, StaticMeshRenderingDS, rg::RGDescriptorSetState<StaticMeshRenderingDS>, DS_STAGES(rhi::EShaderStageFlags::Vertex, rhi::EShaderStageFlags::Fragment))
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<StaticMeshDrawCallData>),	drawCommands)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<math::Matrix4f>),			instanceTransforms)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneViewData>),				sceneViewData)
DS_END();


namespace utils
{

StaticMeshIndirectDataPerView CreateIndirectDataForView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene)
{
	SPT_PROFILER_FUNCTION();

	const rhi::BufferDefinition drawsBufferDef(sizeof(StaticMeshDrawCallData) * 1024, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect));
	const rhi::RHIAllocationInfo drawsBufferAllocation(rhi::EMemoryUsage::GPUOnly);
	StaticMeshIndirectDataPerView indirectData;
	indirectData.drawsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("StaticMeshDrawsBuffer"), drawsBufferDef, drawsBufferAllocation);

	const rhi::BufferDefinition countBufferDef(sizeof(Uint32), lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect));
	const rhi::RHIAllocationInfo countBufferAllocation(rhi::EMemoryUsage::CPUToGPU);
	indirectData.countBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("StaticMeshCountBuffer"), countBufferDef, countBufferAllocation);

	graphBuilder.AddFillBuffer(RG_DEBUG_NAME("Initialize Mesh Instances Count"), indirectData.countBuffer, 0, sizeof(Uint32), 0);

	return indirectData;
}

} // utils

StaticMeshesRenderSystem::StaticMeshesRenderSystem()
{
	m_supportedStages = lib::Flags(ERenderStage::GBufferGenerationStage, ERenderStage::ShadowGenerationStage);

	{
		sc::ShaderCompilationSettings compilationSettings;
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "GenerateCommandsCS"));
		const rdr::ShaderID generateCommandsShader = rdr::ResourcesManager::CreateShader("StaticMeshes/GenerateIndirectCommands.hlsl", compilationSettings);
		indirectCommandsGenerationPipeline = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("GenerateIndirectCommandsPipeline"), generateCommandsShader);
	}

	{
		sc::ShaderCompilationSettings compilationSettings;
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Vertex, "StaticMeshVS"));
		compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Fragment, "StaticMeshFS"));
		const rdr::ShaderID generateGBufferShader = rdr::ResourcesManager::CreateShader("StaticMeshes/GenerateGBuffer_StaticMesh.hlsl", compilationSettings);

		rhi::GraphicsPipelineDefinition pipelineDef;
		pipelineDef.primitiveTopology = rhi::EPrimitiveTopology::TriangleList;
		pipelineDef.renderTargetsDefinition.depthRTDefinition.format = rhi::EFragmentFormat::D32_S_Float;
		pipelineDef.renderTargetsDefinition.colorRTsDefinition.emplace_back(rhi::ColorRenderTargetDefinition(rhi::EFragmentFormat::RGBA8_UN_Float));

		generateGBufferPipeline = rdr::ResourcesManager::CreateGfxPipeline(RENDERER_RESOURCE_NAME("GenerateGBuffer_StaticMesh"), pipelineDef, generateGBufferShader);
	}
}

void StaticMeshesRenderSystem::RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	Super::RenderPerView(graphBuilder, renderScene, viewSpec);

	const StaticMeshPrimitivesSystem& staticMeshPrimsSystem = renderScene.GetPrimitivesSystemChecked<StaticMeshPrimitivesSystem>();
	const lib::SharedRef<rdr::Buffer>& instancesBuffer = staticMeshPrimsSystem.GetStaticMeshInstances().GetBuffer();

	const StaticMeshIndirectDataPerView& viewIndirectData = viewSpec.GetData().Create<StaticMeshIndirectDataPerView>(utils::CreateIndirectDataForView(graphBuilder, renderScene));

	lib::SharedRef<BuildIndirectStaticMeshCommandsDS> descriptorSetState = rdr::ResourcesManager::CreateDescriptorSetState<BuildIndirectStaticMeshCommandsDS>(RENDERER_RESOURCE_NAME("IndirectCommandsDS"));
	descriptorSetState->drawCommands = viewIndirectData.drawsBuffer;
	descriptorSetState->drawCommandsCount = viewIndirectData.countBuffer;
	descriptorSetState->staticMeshes = instancesBuffer->CreateFullView();
	descriptorSetState->instanceTransforms = renderScene.GetTransformsBuffer()->CreateFullView();

	graphBuilder.AddDispatch(RG_DEBUG_NAME("Generate Static Mesh Indirect Command"),
							 indirectCommandsGenerationPipeline,
							 math::Vector3u(1, 1, 1),
							 rg::BindDescriptorSets(descriptorSetState, lib::Ref(GeometryManager::Get().GetPrimitivesDSState())));

	if (viewSpec.SupportsStage(ERenderStage::GBufferGenerationStage))
	{
		RenderStageEntries& basePassStageEntries = viewSpec.GetRenderStageEntries(ERenderStage::GBufferGenerationStage);

		basePassStageEntries.GetOnRenderStage().AddRawMember(this, &StaticMeshesRenderSystem::RenderMeshesPerView);
	}
}

void StaticMeshesRenderSystem::RenderMeshesPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context)
{
	SPT_PROFILER_FUNCTION();

	const StaticMeshIndirectDataPerView& indirectBuffers = viewSpec.GetData().Get<StaticMeshIndirectDataPerView>();

	const lib::SharedRef<StaticMeshRenderingDS> staticMeshRenderingDS = rdr::ResourcesManager::CreateDescriptorSetState<StaticMeshRenderingDS>(RENDERER_RESOURCE_NAME("StaticMeshRenderingDS"));
	staticMeshRenderingDS->drawCommands = indirectBuffers.drawsBuffer;
	staticMeshRenderingDS->instanceTransforms = renderScene.GetTransformsBuffer()->CreateFullView();
	staticMeshRenderingDS->sceneViewData = viewSpec.GetRenderView().GenerateViewData();

	const lib::SharedRef<GeometryDS> unifiedGeometryDS = lib::Ref(GeometryManager::Get().GetGeometryDSState());

	graphBuilder.AddSubpass(RG_DEBUG_NAME("Render Static Meshes"),
							rg::BindDescriptorSets(staticMeshRenderingDS, unifiedGeometryDS),
							std::tie(indirectBuffers),
							[&indirectBuffers, &viewSpec, this](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.BindGraphicsPipeline(generateGBufferPipeline);

								const Uint32 maxInstances = 1024;

								const math::Vector2u renderingArea = viewSpec.GetRenderView().GetRenderingResolution();

								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), renderingArea.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), renderingArea));

								const rdr::BufferView& drawsBufferView = indirectBuffers.drawsBuffer->GetBufferViewInstance();
								const rdr::BufferView& drawsCountBufferView = indirectBuffers.countBuffer->GetBufferViewInstance();
								recorder.DrawIndirect(drawsBufferView, 0, sizeof(StaticMeshDrawCallData), drawsCountBufferView, 0, maxInstances);
							});
}

} // spt::rsc
