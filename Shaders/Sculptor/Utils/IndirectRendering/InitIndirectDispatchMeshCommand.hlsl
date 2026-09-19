#include "SculptorShader.hlsli"

[[shader_params(InitIndirectDispatchMeshCommand, PARAMS_INIT_INDIRECT_DISPATCH_MESH_COMMAND)]]


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(1, 1, 1)]
void InitIndirectDispatchMeshCommandCS(CS_INPUT input)
{
	IndirectDispatchCommand dispatchCommand;
	dispatchCommand.threadGroupsX = (PARAMS_INIT_INDIRECT_DISPATCH_MESH_COMMAND->instancesCountBuffer.Load(0u) + PARAMS_INIT_INDIRECT_DISPATCH_MESH_COMMAND->instancesPerGroup - 1u) / PARAMS_INIT_INDIRECT_DISPATCH_MESH_COMMAND->instancesPerGroup;
	dispatchCommand.threadGroupsY = 1u;
	dispatchCommand.threadGroupsZ = 1u;
	dispatchCommand.padding       = 0u;

	PARAMS_INIT_INDIRECT_DISPATCH_MESH_COMMAND->outDrawCommand.Store(0u, dispatchCommand);
}
