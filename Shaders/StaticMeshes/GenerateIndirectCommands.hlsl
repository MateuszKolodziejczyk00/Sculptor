#include "SculptorShader.hlsli"

[[descriptor_set(BuildIndirectStaticMeshCommandsDS, 0)]]
[[descriptor_set(PrimitivesDS, 1)]]


groupshared uint outputIdx;


[numthreads(1024, 1, 1)]
void GenerateCommandsCS(uint3 id : SV_DispatchThreadID)
{
    // Initialize idx value
    if(id.x == 0)
    {
        outputIdx = 0;
    }

    GroupMemoryBarrier();

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
            drawCommands[commandIdx].transformIdx = staticMeshes[id.x].transformIdx;
        }
    }

    // Wait for all threads to output idx
    GroupMemoryBarrier();

    if(id.x == 0)
    {
        drawCommandsCount[0] = outputIdx;
    }
}