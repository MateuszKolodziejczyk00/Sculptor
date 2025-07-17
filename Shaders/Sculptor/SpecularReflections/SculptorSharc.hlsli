#ifndef SCULPTOR_SHARC_HLSLI
#define SCULPTOR_SHARC_HLSLI

#define SHARC_ENABLE_64_BIT_ATOMICS 1
#define SHARC_MK_CHANGES 1


#include "Sharc/SharcCommon.h"


struct SharcDef
{
    float3 cameraPosition;
    uint capacity;
    float exposure;
    RW_STRUCTURED_BUFFER(hashEntries,   uint64_t);
    RW_STRUCTURED_BUFFER(voxelData,     uint4);
    RW_STRUCTURED_BUFFER(voxelDataPrev, uint4);
};


HashGridParameters CreateHashGridParameters(in float3 viewLocation)
{
    HashGridParameters grid;
    grid.cameraPosition = viewLocation;
    grid.logarithmBase  = SHARC_GRID_LOGARITHM_BASE;
    grid.sceneScale     = 50.f;
    grid.levelBias      = SHARC_GRID_LEVEL_BIAS;
    return grid;
}


SharcParameters CreateSharcParameters(in SharcDef def)
{
    HashGridParameters grid = CreateHashGridParameters(def.cameraPosition);

    HashMapData data;
    data.capacity          = def.capacity;
    data.hashEntriesBuffer = def.hashEntries;

    SharcParameters params;
    params.gridParameters          = grid;
    params.hashMapData             = data;
    params.voxelDataBuffer         = def.voxelData;
    params.voxelDataBufferPrev     = def.voxelDataPrev;
    params.enableAntiFireflyFilter = true;
    params.exposure                = def.exposure;

    return params;
}

#endif // SCULPTOR_SHARC_HLSLI
