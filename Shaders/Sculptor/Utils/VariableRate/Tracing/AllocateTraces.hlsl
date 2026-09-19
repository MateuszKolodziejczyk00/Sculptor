#include "SculptorShader.hlsli"

[[shader_params(AllocateTracesConstants, PARAMS_ALLOCATE_TRACES)]]

#include "Utils/VariableRate/Tracing/TracesAllocator.hlsli"


struct CS_INPUT
{
	uint3 groupID : SV_GroupID;
	uint3 localID : SV_GroupThreadID;
};


[numthreads(TRACES_ALLOCATOR_GROUP_X , TRACES_ALLOCATOR_GROUP_Y , 1)]
void AllocateTracesCS(CS_INPUT input)
{
	uint2 localID = input.localID.xy;
	ReorderLocalIDForAllocating(INOUT localID);

	bool isHelperLane = false;
	uint2 globalID = input.groupID.xy * uint2(TRACES_ALLOCATOR_GROUP_X, TRACES_ALLOCATOR_GROUP_Y) + localID;

	if(any(globalID >= PARAMS_ALLOCATE_TRACES->resolution))
	{
		isHelperLane = true;
		globalID = min(globalID, PARAMS_ALLOCATE_TRACES->resolution - 1);
	}

#if VR_USE_LARGE_TILE
	const uint2 variableRateCoords = globalID / 8;
#else
	const uint2 variableRateCoords = globalID / 2;
#endif // VR_USE_LARGE_TILE

	const uint variableRate = LoadVariableRate(PARAMS_ALLOCATE_TRACES->variableRateTexture, variableRateCoords);

	TracesAllocator tracesAllocator = TracesAllocator::Create(PARAMS_ALLOCATE_TRACES->rayTracesCommands, PARAMS_ALLOCATE_TRACES->commandsNum, PARAMS_ALLOCATE_TRACES->rwVariableRateBlocksTexture);
#if OUTPUT_TRACES_AND_DISPATCH_GROUPS_NUM
	tracesAllocator.SetTracesNumBuffers(PARAMS_ALLOCATE_TRACES->tracesNum, PARAMS_ALLOCATE_TRACES->tracesDispatchGroupsNum);
#endif // OUTPUT_TRACES_AND_DISPATCH_GROUPS_NUM

	const bool maskOutOutput = isHelperLane;
	tracesAllocator.AllocateTraces(input.groupID.xy, localID, variableRate, PARAMS_ALLOCATE_TRACES->traceIdx, maskOutOutput);
}