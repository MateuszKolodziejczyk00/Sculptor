#include "Utils/IndirectUtils.h"
#include "Bindless/BindlessTypes.h"
#include "ResourcesManager.h"
#include "RenderGraphBuilder.h"
#include "Pipelines/PSOsLibraryTypes.h"


namespace spt::gfx
{

namespace indirect_utils
{

rg::RGBufferViewHandle CreateCounterBuffer(rg::RenderGraphBuilder& graphBuilder, Uint32 initialValue)
{
	rg::RGBufferViewHandle counterBuffer = graphBuilder.CreateStorageBufferView(RG_DEBUG_NAME("Indirect Draw Counter Buffer"), sizeof(Uint32));

	graphBuilder.FillBuffer(RG_DEBUG_NAME("Initialize Indirect Draw Counter Buffer"), counterBuffer, 0, sizeof(Uint32), initialValue);

	return counterBuffer;
}


SIMPLE_COMPUTE_PSO(InitIndirectInstancedDrawCommandPSO, "Sculptor/Utils/IndirectRendering/InitIndirectInstancedDrawCommand.hlsl", InitIndirectInstancedDrawCommandCS);


BEGIN_SHADER_STRUCT(InitIndirectInstancedDrawCommandConstants)
	SHADER_STRUCT_FIELD(Uint32,                                     vertexCountPerInstance)
	SHADER_STRUCT_FIELD(Uint32,                                     startVertexLocation)
	SHADER_STRUCT_FIELD(Uint32,                                     startInstanceLocation)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<Uint32>,                instancesCountBuffer)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<IndirectDrawCommand>, outDrawCommand)
END_SHADER_STRUCT();


rg::RGBufferViewHandle CreateIndirectInstancedDrawCommand(rg::RenderGraphBuilder& graphBuilder, rg::RGBufferViewHandle instancesCount, Uint32 verticesPerInstance, Uint32 firstVertex /* = 0 */, Uint32 firstInstance /* = 0 */)
{
	const rg::RGBufferViewHandle commandBuffer = graphBuilder.CreateStorageBufferView(RG_DEBUG_NAME("Indirect Instanced Draw Command Buffer"), sizeof(IndirectDrawCommand));

	InitIndirectInstancedDrawCommandConstants shaderConstants;
	shaderConstants.vertexCountPerInstance = verticesPerInstance;
	shaderConstants.startVertexLocation = firstVertex;
	shaderConstants.startInstanceLocation = firstInstance;
	shaderConstants.instancesCountBuffer = instancesCount;
	shaderConstants.outDrawCommand = commandBuffer;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Initialize Indirect Instanced Draw Command"),
						  InitIndirectInstancedDrawCommandPSO::pso,
						  1u,
						  rg::EmptyDescriptorSets(),
						  shaderConstants);

	return commandBuffer;
}

} // indirect_utils

} // spt::gfx
