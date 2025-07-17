#ifndef SCULPTOR_SHARC_QUERY_HLSLI
#define SCULPTOR_SHARC_QUERY_HLSLI

#include "SpecularReflections/SculptorSharc.hlsli"

#ifdef DS_SharcCacheDS

bool QueryCachedLuminance(in float3 viewLocation, in float exposure, in float3 location, in float3 normal, out float3 luminance)
{
    SharcDef sharcDef;
    sharcDef.cameraPosition = viewLocation;
    sharcDef.capacity       = u_sharcCacheConstants.entriesNum;
    sharcDef.hashEntries    = u_hashEntries;
    sharcDef.voxelData      = u_voxelData;
    sharcDef.exposure       = exposure;

    const SharcParameters sharcParams = CreateSharcParameters(sharcDef);

    SharcHitData hitData;
    hitData.positionWorld = location;
    hitData.normalWorld   = normal;
    const bool success = SharcGetCachedRadiance(sharcParams, hitData, OUT luminance, false);

    return success;
}

#endif // DS_SharcCacheDS

//#undef RW_STRUCTURED_BUFFER

#endif // SCULPTOR_SHARC_QUERY_HLSLI