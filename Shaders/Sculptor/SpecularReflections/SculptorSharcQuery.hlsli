#ifndef SCULPTOR_SHARC_QUERY_HLSLI
#define SCULPTOR_SHARC_QUERY_HLSLI

#include "SpecularReflections/SculptorSharc.hlsli"

#ifdef DS_SharcCacheDS

SharcParameters CreateSharcParameters(in float3 viewLocation, in float exposure)
{
    SharcDef sharcDef;
    sharcDef.cameraPosition = viewLocation;
    sharcDef.capacity       = u_sharcCacheConstants.entriesNum;
    sharcDef.hashEntries    = u_hashEntries;
    sharcDef.voxelData      = u_voxelData;
    sharcDef.exposure       = exposure;

    return CreateSharcParameters(sharcDef);
}


bool QueryCachedLuminance(in SharcParameters sharcParams, in float3 materialDemodulation, in float3 location, in float3 normal, out float3 luminance)
{
    SharcHitData hitData;
    hitData.positionWorld        = location;
    hitData.normalWorld          = normal;
    hitData.materialDemodulation = materialDemodulation;
    const bool success = SharcGetCachedRadiance(sharcParams, hitData, OUT luminance, false);

    return success;
}


bool QueryCachedLuminance(in float3 viewLocation, in float exposure, in float3 materialDemodulation, in float3 location, in float3 normal, out float3 luminance)
{
    const SharcParameters sharcParams = CreateSharcParameters(viewLocation, exposure);

    return QueryCachedLuminance(sharcParams, materialDemodulation, location, normal, OUT luminance);
}

#endif // DS_SharcCacheDS

//#undef RW_STRUCTURED_BUFFER

#endif // SCULPTOR_SHARC_QUERY_HLSLI