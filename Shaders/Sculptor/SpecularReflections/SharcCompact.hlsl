#include "SculptorShader.hlsli"

[[descriptor_set(SharcCompactDS, 0)]]


#include "SpecularReflections/SculptorSharc.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(64, 1, 1)]
void SharcCompactCS(CS_INPUT input)
{
    HashMapData hashMap;
    hashMap.capacity          = u_constants.sharcCapacity;
    hashMap.hashEntriesBuffer = u_hashEntries;

    SharcCopyHashEntry(input.globalID.x, hashMap, u_copyOffset);
}
