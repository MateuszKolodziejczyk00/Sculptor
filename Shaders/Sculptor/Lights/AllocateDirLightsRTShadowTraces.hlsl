#include "SculptorShader.hlsli"

[[descriptor_set(TracesAllocatorDS, 0)]]
[[descriptor_set(DirLightShadowRaysAllocatorDS, 1)]]
[[descriptor_set(RenderViewDS, 2)]]

#include "Utils/VariableRate/Tracing/TracesAllocator.hlsli"


struct CS_INPUT
{
	uint3 groupID : SV_GroupID;
	uint3 localID : SV_GroupThreadID;
};


[numthreads(TRACES_ALLOCATOR_GROUP_X , TRACES_ALLOCATOR_GROUP_Y , 1)]
void AllocateDirLightsRTShadowTracesCS(CS_INPUT input)
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

	const uint variableRate = LoadVariableRate(u_variableRateTexture, globalID / 2);

	TracesAllocator tracesAllocator = TracesAllocator::Create(u_rayTracesCommands, u_commandsNum);

	const bool maskOutOutput = isHelperLane;
	tracesAllocator.AllocateTraces(input.groupID.xy, localID, variableRate, maskOutOutput);
}
