#include "SculptorShader.hlsli"

[[shader_params(InitIndirectInstancedDrawCommandConstants, u_constants)]]

[[shader_struct(IndirectDrawCommand)]]


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(1, 1, 1)]
void InitIndirectInstancedDrawCommandCS(CS_INPUT input)
{
	IndirectDrawCommand drawCommand;
	drawCommand.vertexCount   = u_constants.vertexCountPerInstance;
	drawCommand.instanceCount = u_constants.instancesCountBuffer.Load(0u);
	drawCommand.firstVertex   = u_constants.startVertexLocation;
	drawCommand.firstInstance = u_constants.startInstanceLocation;

	u_constants.outDrawCommand.Store(0u, drawCommand);
}
