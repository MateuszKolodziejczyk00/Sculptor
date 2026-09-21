#ifndef SCULPTOR_SHARC_QUERY_HLSLI
#define SCULPTOR_SHARC_QUERY_HLSLI

#include "SpecularReflections/SculptorSharc.hlsli"

#ifdef PARAM_SharcCacheParams

SharcParameters CreateSharcParameters(in float3 viewLocation, in float exposure)
{
    SharcDef sharcDef;
    sharcDef.cameraPosition = viewLocation;
    sharcDef.capacity       = PARAM_SharcCacheParams->entriesNum;
    sharcDef.hashEntries    = PARAM_SharcCacheParams->hashEntries.GetResource();
    sharcDef.voxelData      = PARAM_SharcCacheParams->voxelData.GetResource();
    sharcDef.exposure       = exposure;

    return CreateSharcParameters(sharcDef);
}


struct SharcQuery
{
#if SHARC_MATERIAL_DEMODULATION
	float3 materialDemodulation;
#endif // SHARC_MATERIAL_DEMODULATION
	float3 location;
	float3 normal;
#if SHARC_SEPARATE_EMISSIVE
	float  emissive;
#endif // SHARC_SEPARATE_EMISSIVE
};


bool QueryCachedLuminance(in SharcParameters sharcParams, in SharcQuery query, out float3 luminance)
{
    SharcHitData hitData;
    hitData.positionWorld        = query.location;
    hitData.normalWorld          = query.normal;
#if SHARC_MATERIAL_DEMODULATION
    hitData.materialDemodulation = query.materialDemodulation;
#endif // SHARC_MATERIAL_DEMODULATION
#if SHARC_SEPARATE_EMISSIVE
	hitData.emissive             = query.emissive;
#endif // SHARC_SEPARATE_EMISSIVE
    const bool success = SharcGetCachedRadiance(sharcParams, hitData, OUT luminance, false);

    return success;
}


bool QueryCachedLuminance(in float3 viewLocation, in float exposure, in SharcQuery query, out float3 luminance)
{
    const SharcParameters sharcParams = CreateSharcParameters(viewLocation, exposure);

    return QueryCachedLuminance(sharcParams, query, OUT luminance);
}

#endif // PARAM_SharcCacheParams

//#undef RW_STRUCTURED_BUFFER

#endif // SCULPTOR_SHARC_QUERY_HLSLI
