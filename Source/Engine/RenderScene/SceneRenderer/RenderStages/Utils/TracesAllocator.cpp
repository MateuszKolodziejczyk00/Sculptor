#include "TracesAllocator.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "GlobalResources/GlobalResources.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"


namespace spt::rsc::vrt
{

BEGIN_SHADER_STRUCT(AllocateTracesConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, invResolution)
	SHADER_STRUCT_FIELD(math::Vector2u, vrtResolution)
	SHADER_STRUCT_FIELD(math::Vector2f, vrtInvResolution)
	SHADER_STRUCT_FIELD(Uint32,         traceIdx)
	SHADER_STRUCT_FIELD(Bool,           enableBlueNoiseLocalOffset)
END_SHADER_STRUCT();


DS_BEGIN(AllocateTracesDS, rg::RGDescriptorSetState<AllocateTracesDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                       u_variableRateTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                       u_blueNoise256Texture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<AllocateTracesConstants>),    u_constants)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<EncodedRayTraceCommand>), u_rayTracesCommands)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                 u_commandsNum)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Uint32>),                        u_rwVariableRateBlocksTexture)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWStructuredBufferBinding<Uint32>),         u_tracesDispatchGroupsNum)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWStructuredBufferBinding<Uint32>),         u_tracesNum)
DS_END();


static rdr::PipelineStateID CreateAllocateTracesPipeline(const vrt::VariableRatePermutationSettings& permutationSettings, Bool outputTracesAndDispatchGroupsNum)
{
	sc::ShaderCompilationSettings compilationSettings;
	vrt::ApplyVariableRatePermutation(INOUT compilationSettings, permutationSettings);
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("OUTPUT_TRACES_AND_DISPATCH_GROUPS_NUM", outputTracesAndDispatchGroupsNum ? "1" : "0"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Utils/VariableRate/Tracing/AllocateTraces.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "AllocateTracesCS"), compilationSettings);
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("AllocateTracesPipeline"), shader);
}

TracesAllocation AllocateTraces(rg::RenderGraphBuilder& graphBuilder, const TracesAllocationDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(definition.variableRateTexture.IsValid());

	TracesAllocation tracesAllocation;

	{
		const Uint32 tracesNum = definition.resolution.x() * definition.resolution.y();

		rhi::BufferDefinition bufferDef;
		bufferDef.size = tracesNum * sizeof(EncodedRayTraceCommand);;
		bufferDef.usage = rhi::EBufferUsage::Storage;
		tracesAllocation.rayTraceCommands = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Ray Trace Commands Buffer"), bufferDef, rhi::EMemoryUsage::GPUOnly);
	}

	{
		rhi::BufferDefinition bufferDef;
		bufferDef.size = sizeof(math::Vector4u);
		bufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect, rhi::EBufferUsage::DeviceAddress, rhi::EBufferUsage::TransferDst);
		tracesAllocation.tracingIndirectArgs = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Ray Tracing Commands Num Buffer"), bufferDef, rhi::EMemoryUsage::GPUOnly);

		graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Clear Trace Commands Num"), tracesAllocation.tracingIndirectArgs, 0u);
	}

	if (definition.outputTracesAndDispatchGroupsNum)
	{
		{
			rhi::BufferDefinition bufferDef;
			bufferDef.size = sizeof(math::Vector4u);
			bufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect, rhi::EBufferUsage::TransferDst);
			tracesAllocation.dispatchIndirectArgs = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Traces Dispatch Groups Num Buffer"), bufferDef, rhi::EMemoryUsage::GPUOnly);

			graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Clear Traces Dispatch Groups Num"), tracesAllocation.dispatchIndirectArgs, 1u);
		}

		{
			rhi::BufferDefinition bufferDef;
			bufferDef.size = sizeof(Uint32);
			bufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferSrc, rhi::EBufferUsage::TransferDst);
			tracesAllocation.tracesNum = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Traces Num Buffer"), bufferDef, rhi::EMemoryUsage::GPUOnly);

			graphBuilder.FillFullBuffer(RG_DEBUG_NAME("Clear Traces Num"), tracesAllocation.tracesNum, 0u);
		}
	}

	tracesAllocation.variableRateBlocksTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Variable Rate Blocks Texture"), rg::TextureDef(definition.resolution, rhi::EFragmentFormat::R8_U_Int));

	const math::Vector2u vrtResolution = definition.variableRateTexture->GetResolution2D();

	AllocateTracesConstants shaderConstants;
	shaderConstants.resolution                 = definition.resolution;
	shaderConstants.invResolution              = definition.resolution.cast<Real32>().cwiseInverse();
	shaderConstants.vrtResolution              = vrtResolution;
	shaderConstants.vrtInvResolution           = vrtResolution.cast<Real32>().cwiseInverse();
	shaderConstants.traceIdx                   = definition.traceIdx;
	shaderConstants.enableBlueNoiseLocalOffset = definition.enableBlueNoiseLocalOffset;

	lib::MTHandle<AllocateTracesDS> allocateTracesDS = graphBuilder.CreateDescriptorSet<AllocateTracesDS>(RENDERER_RESOURCE_NAME("AllocateTracesDS"));
	allocateTracesDS->u_constants                   = shaderConstants;
	allocateTracesDS->u_variableRateTexture         = definition.variableRateTexture;
	allocateTracesDS->u_blueNoise256Texture         = gfx::global::Resources::Get().blueNoise256.GetView();
	allocateTracesDS->u_rayTracesCommands           = tracesAllocation.rayTraceCommands;
	allocateTracesDS->u_commandsNum                 = tracesAllocation.tracingIndirectArgs;
	allocateTracesDS->u_rwVariableRateBlocksTexture = tracesAllocation.variableRateBlocksTexture;

	if (definition.outputTracesAndDispatchGroupsNum)
	{
		allocateTracesDS->u_tracesDispatchGroupsNum = tracesAllocation.dispatchIndirectArgs;
		allocateTracesDS->u_tracesNum               = tracesAllocation.tracesNum;
	}

	const rdr::PipelineStateID allocateTracesPipeline = CreateAllocateTracesPipeline(definition.vrtPermutationSettings, definition.outputTracesAndDispatchGroupsNum);

	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("Allocate Traces: {}", definition.debugName.AsString().data()),
						  allocateTracesPipeline,
						  math::Utils::DivideCeil(definition.resolution, math::Vector2u(16u, 16u)),
						  rg::BindDescriptorSets(allocateTracesDS));

	return tracesAllocation;
}

} // spt::rsc::vrt
