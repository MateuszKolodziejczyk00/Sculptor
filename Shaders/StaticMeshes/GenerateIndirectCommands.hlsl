#include "SculptorShader.hlsli"

[[descriptor_set(BuildIndirectStaticMeshCommandsDS, 0)]]
[[descriptor_set(PrimitivesDS, 1)]]

[[shader_struct(StaticMeshGPURenderData)]]
[[shader_struct(MeshletData)]]
[[shader_struct(SubmeshData)]]

#define SUBMESH_DATA_SIZE 12

groupshared uint outputIdx;


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
    uint3 groupID : SV_GroupID;
};


[numthreads(32, 1, 1)]
void GenerateCommandsCS(CS_INPUT input)
{
    // Initialize idx value
    if(input.globalID.x == 0)
    {
        outputIdx = 0;
    }

    GroupMemoryBarrier();
    
    const uint staticMeshDataOffset = staticMeshes[0].staticMeshDataOffset;
    const StaticMeshGPURenderData staticMesh = primitivesRenderDataData.Load<StaticMeshGPURenderData>(staticMeshDataOffset);

    const uint submeshOffset = staticMesh.submeshesOffset + input.groupID * SUBMESH_DATA_SIZE;
    const SubmeshData submesh = primitivesRenderDataData.Load<SubmeshData>(submeshOffset);

    const uint meshletsOffset = staticMesh.meshletsOffset;

    

    //const uint commandsNum = staticMeshes[id.x].primitivesNum;
    //if(commandsNum > 0)
    //{
    //    uint commandsBeginIdx = 0;
    //    InterlockedAdd(outputIdx, commandsNum, commandsBeginIdx);

    //    for (uint idx = 0; idx < commandsNum; ++idx)
    //    {
    //        const uint primitiveIdx = staticMeshes[id.x].firstPrimitiveIdx + idx;
    //        const uint commandIdx = commandsBeginIdx + idx;

    //        drawCommands[commandIdx].vertexCount = primitivesData[primitiveIdx].indicesNum;
    //        drawCommands[commandIdx].instanceCount = 1;
    //        drawCommands[commandIdx].firstVertex = 0;
    //        drawCommands[commandIdx].firstInstance = 0;
    //        drawCommands[commandIdx].primitiveIdx = primitiveIdx;
    //        drawCommands[commandIdx].transformIdx = staticMeshes[id.x].transformIdx;
    //    }
    //}

    // Wait for all threads to output idx
    GroupMemoryBarrier();

    if(input.globalID.x == 0)
    {
        drawCommandsCount[0] = outputIdx;
    }
}