#include "SculptorShader.hlsli"

[[descriptor_set(LuminanceHistogramDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"

// Based on https://bruop.github.io/exposure/


#define EPSILON 0.005


struct CS_INPUT
{
    uint3 globalID  : SV_DispatchThreadID;
    uint3 localID   : SV_GroupThreadID;
};


groupshared uint groupHistogramBins[256];


uint LuminanceToBinIdx(float luminance, float minLogLuminance, float inverseLogLuminanceRange)
{
    // Avoid taking the log of zero
    if (luminance < EPSILON)
    {
        return 0;
    }
    
    const float logLuminance = saturate((log2(luminance) - minLogLuminance) * inverseLogLuminanceRange);
    
    // Map [0, 1] to [1, 255]. The zeroth bin is handled by the epsilon check above.
    return uint(logLuminance * 254.0 + 1.0);
}


[numthreads(16, 16, 1)]
void LuminanceHistogramCS(CS_INPUT input)
{
    const uint histogramLocalBinIdx = input.localID.x + input.localID.y * 16;
    groupHistogramBins[histogramLocalBinIdx] = 0;

    GroupMemoryBarrierWithGroupSync();
    
    const uint2 pixel = input.globalID.xy * 2u;

    if(pixel.x < u_exposureSettings.textureSize.x && pixel.y < u_exposureSettings.textureSize.y)
    {
        const float2 uv = pixel * u_exposureSettings.inputPixelSize + u_exposureSettings.inputPixelSize;
        const float4 rChannel4 = u_linearColorTexture.GatherRed(u_sampler, uv, 0);
        const float4 gChannel4 = u_linearColorTexture.GatherGreen(u_sampler, uv, 0);
        const float4 bChannel4 = u_linearColorTexture.GatherBlue(u_sampler, uv, 0);

        const float rcpExposure = rcp(GetViewExposure());

        const float lum0 = Luminance(float3(rChannel4.x, gChannel4.x, bChannel4.x)) * rcpExposure;
        const float lum1 = Luminance(float3(rChannel4.y, gChannel4.y, bChannel4.y)) * rcpExposure;
        const float lum2 = Luminance(float3(rChannel4.z, gChannel4.z, bChannel4.z)) * rcpExposure;
        const float lum3 = Luminance(float3(rChannel4.w, gChannel4.w, bChannel4.w)) * rcpExposure;

        const uint binIdx0 = LuminanceToBinIdx(lum0, u_exposureSettings.minLogLuminance, u_exposureSettings.inverseLogLuminanceRange);
        const uint binIdx1 = LuminanceToBinIdx(lum1, u_exposureSettings.minLogLuminance, u_exposureSettings.inverseLogLuminanceRange);
        const uint binIdx2 = LuminanceToBinIdx(lum2, u_exposureSettings.minLogLuminance, u_exposureSettings.inverseLogLuminanceRange);
        const uint binIdx3 = LuminanceToBinIdx(lum3, u_exposureSettings.minLogLuminance, u_exposureSettings.inverseLogLuminanceRange);

        InterlockedAdd(groupHistogramBins[binIdx0], 1);
        InterlockedAdd(groupHistogramBins[binIdx1], 1);
        InterlockedAdd(groupHistogramBins[binIdx2], 1);
        InterlockedAdd(groupHistogramBins[binIdx3], 1);

    }

    GroupMemoryBarrierWithGroupSync();

    InterlockedAdd(u_luminanceHistogram[histogramLocalBinIdx], groupHistogramBins[histogramLocalBinIdx]);
}