#include "SculptorShader.hlsli"

[[shader_params(InitIndirectInstancedDrawCommandConstants, PARAMS_INIT_INDIRECT_INSTANCED_DRAW_COMMAND_CONSTANTS)]]

[[shader_struct(IndirectDrawCommand)]]


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(1, 1, 1)]
void InitIndirectInstancedDrawCommandCS(CS_INPUT input)
{
	IndirectDrawCommand drawCommand;
	drawCommand.vertexCount   = PARAMS_INIT_INDIRECT_INSTANCED_DRAW_COMMAND_CONSTANTS->vertexCountPerInstance;
	drawCommand.instanceCount = PARAMS_INIT_INDIRECT_INSTANCED_DRAW_COMMAND_CONSTANTS->instancesCountBuffer.Load(0u);
	drawCommand.firstVertex   = PARAMS_INIT_INDIRECT_INSTANCED_DRAW_COMMAND_CONSTANTS->startVertexLocation;
	drawCommand.firstInstance = PARAMS_INIT_INDIRECT_INSTANCED_DRAW_COMMAND_CONSTANTS->startInstanceLocation;

	PARAMS_INIT_INDIRECT_INSTANCED_DRAW_COMMAND_CONSTANTS->outDrawCommand.Store(0u, drawCommand);
}
