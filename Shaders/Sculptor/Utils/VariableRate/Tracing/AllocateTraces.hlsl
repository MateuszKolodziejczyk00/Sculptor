#include "SculptorShader.hlsli"

[[descriptor_set(AllocateTracesDS, 0)]]

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

	if(any(globalID >= u_constants.resolution))
	{
		isHelperLane = true;
		globalID = min(globalID, u_constants.resolution - 1);
	}

#if VR_USE_LARGE_TILE
	const uint2 variableRateCoords = globalID / 8;
#else
	const uint2 variableRateCoords = globalID / 2;
#endif // VR_USE_LARGE_TILE

	const uint variableRate = LoadVariableRate(u_variableRateTexture, variableRateCoords);

	TracesAllocator tracesAllocator = TracesAllocator::Create(u_rayTracesCommands, u_commandsNum, u_rwVariableRateBlocksTexture);
#if OUTPUT_TRACES_AND_DISPATCH_GROUPS_NUM
	tracesAllocator.SetTracesNumBuffers(u_tracesNum, u_tracesDispatchGroupsNum);
#endif // OUTPUT_TRACES_AND_DISPATCH_GROUPS_NUM

	const bool maskOutOutput = isHelperLane;
	tracesAllocator.AllocateTraces(input.groupID.xy, localID, variableRate, u_constants.traceIdx, maskOutOutput);
}