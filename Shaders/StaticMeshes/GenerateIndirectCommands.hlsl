#type(compute) //===========================================================
#include "SculptorShader.hlsli"

[[descriptor_set(BuildIndirectStaticMeshCommandsDS, 0)]]
[[descriptor_set(PrimitivesDS, 1)]]


groupshared uint outputIdx;


[numthreads(1024, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    const uint commandsNum = staticMeshes[id.x].primitivesNum;
    if(commandsNum > 0)
    {
        uint commandsBeginIdx = 0;
        InterlockedAdd(outputIdx, commandsNum, commandsBeginIdx);

        for (uint idx = 0; idx < commandsNum; ++idx)
        {
            const uint primitiveIdx = staticMeshes[id.x].firstPrimitiveIdx + idx;
            const uint commandIdx = commandsBeginIdx + idx;

            drawCommands[commandIdx].vertexCount = primitivesData[primitiveIdx].indicesNum;
            drawCommands[commandIdx].instanceCount = 1;
            drawCommands[commandIdx].firstVertex = 0;
            drawCommands[commandIdx].firstInstance = 0;
            drawCommands[commandIdx].primitiveIdx = primitiveIdx;
        }
    }

    // Wait for all threads to output idx
    GroupMemoryBarrier();

    if(id.x == 0)
    {
        drawCommandsCount[0] = outputIdx;
    }
}