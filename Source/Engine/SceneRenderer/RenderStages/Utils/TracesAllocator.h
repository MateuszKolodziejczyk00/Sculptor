
#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "VariableRateTexture.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

namespace vrt
{

BEGIN_SHADER_STRUCT(EncodedRayTraceCommand)
	/*
		Stores small block 4x4 offset, local offset of trace inside of this block and tracing rate
		This allows to have custom offset within each "lower rate" block for each trace
		Layout:
		[0-11]  block offset X
		[12-23] block offset Y
		[24-25] local offset X
		[26-27] local offset Y
		[28-31] tracing rate
	*/
	SHADER_STRUCT_FIELD(Uint32, coordsAndRate)
END_SHADER_STRUCT();


struct TracesAllocationDefinition
{
	rg::RenderGraphDebugName debugName;

	math::Vector2u                       resolution;
	rg::RGTextureViewHandle              variableRateTexture;
	vrt::VariableRatePermutationSettings vrtPermutationSettings;

	Uint32 traceIdx = 0u;

	Bool outputTracesAndDispatchGroupsNum = false;
};


struct TracesAllocation
{
	TracesAllocation() = default;

	Bool IsValid() const
	{
		return rayTraceCommands.IsValid();
	}

	rg::RGBufferViewHandle rayTraceCommands;
	rg::RGBufferViewHandle tracingIndirectArgs;

	rg::RGTextureViewHandle variableRateBlocksTexture;

	// Assumes 64 threads per group
	rg::RGBufferViewHandle dispatchIndirectArgs;
	rg::RGBufferViewHandle tracesNum;
};

TracesAllocation AllocateTraces(rg::RenderGraphBuilder& graphBuilder, const TracesAllocationDefinition& definition);

} // vrt

} // spt::rsc
