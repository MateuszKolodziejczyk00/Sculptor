#include "SculptorShader.hlsli"

[[shader_params(InitIndirectDispatchMeshCommand, u_constants)]]


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(1, 1, 1)]
void InitIndirectDispatchMeshCommandCS(CS_INPUT input)
{
	IndirectDispatchCommand dispatchCommand;
	dispatchCommand.threadGroupsX = (u_constants.instancesCountBuffer.Load(0u) + u_constants.instancesPerGroup - 1u) / u_constants.instancesPerGroup;
	dispatchCommand.threadGroupsY = 1u;
	dispatchCommand.threadGroupsZ = 1u;
	dispatchCommand.padding       = 0u;

	u_constants.outDrawCommand.Store(0u, dispatchCommand);
}
