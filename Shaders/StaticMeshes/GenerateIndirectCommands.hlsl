#type(compute) //===========================================================
#include "SculptorShader.hlsli"

[[descriptor_set(BuildIndirectStaticMeshCommandsDS, 0)]]


groupshared uint outputIdx;

[numthreads(1024, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{

}