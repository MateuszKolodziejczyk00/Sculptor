#include "SculptorShader.hlsli"

[[shader_params(LuminanceHistogramConstants, PARAMS_LUMINANCE_HISTOGRAM)]]
[[shader_params(GPURenderView, VIEW)]]

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

    if(pixel.x < PARAMS_LUMINANCE_HISTOGRAM->exposureSettings->textureSize.x && pixel.y < PARAMS_LUMINANCE_HISTOGRAM->exposureSettings->textureSize.y)
    {
        const float2 uv = pixel * PARAMS_LUMINANCE_HISTOGRAM->exposureSettings->inputPixelSize + PARAMS_LUMINANCE_HISTOGRAM->exposureSettings->inputPixelSize;
        const float4 rChannel4 = PARAMS_LUMINANCE_HISTOGRAM->linearColorTexture.GatherRed(BindlessSamplers::NearestClampEdge(), uv);
        const float4 gChannel4 = PARAMS_LUMINANCE_HISTOGRAM->linearColorTexture.GatherGreen(BindlessSamplers::NearestClampEdge(), uv);
        const float4 bChannel4 = PARAMS_LUMINANCE_HISTOGRAM->linearColorTexture.GatherBlue(BindlessSamplers::NearestClampEdge(), uv);

        const float rcpExposure = rcp(GetViewExposure());

        const float lum0 = Luminance(float3(rChannel4.x, gChannel4.x, bChannel4.x)) * rcpExposure;
        const float lum1 = Luminance(float3(rChannel4.y, gChannel4.y, bChannel4.y)) * rcpExposure;
        const float lum2 = Luminance(float3(rChannel4.z, gChannel4.z, bChannel4.z)) * rcpExposure;
        const float lum3 = Luminance(float3(rChannel4.w, gChannel4.w, bChannel4.w)) * rcpExposure;

        const uint binIdx0 = LuminanceToBinIdx(lum0, PARAMS_LUMINANCE_HISTOGRAM->exposureSettings->minLogLuminance, PARAMS_LUMINANCE_HISTOGRAM->exposureSettings->inverseLogLuminanceRange);
        const uint binIdx1 = LuminanceToBinIdx(lum1, PARAMS_LUMINANCE_HISTOGRAM->exposureSettings->minLogLuminance, PARAMS_LUMINANCE_HISTOGRAM->exposureSettings->inverseLogLuminanceRange);
        const uint binIdx2 = LuminanceToBinIdx(lum2, PARAMS_LUMINANCE_HISTOGRAM->exposureSettings->minLogLuminance, PARAMS_LUMINANCE_HISTOGRAM->exposureSettings->inverseLogLuminanceRange);
        const uint binIdx3 = LuminanceToBinIdx(lum3, PARAMS_LUMINANCE_HISTOGRAM->exposureSettings->minLogLuminance, PARAMS_LUMINANCE_HISTOGRAM->exposureSettings->inverseLogLuminanceRange);

        InterlockedAdd(groupHistogramBins[binIdx0], 1);
        InterlockedAdd(groupHistogramBins[binIdx1], 1);
        InterlockedAdd(groupHistogramBins[binIdx2], 1);
        InterlockedAdd(groupHistogramBins[binIdx3], 1);

    }

    GroupMemoryBarrierWithGroupSync();

	PARAMS_LUMINANCE_HISTOGRAM->luminanceHistogram.AtomicAdd(histogramLocalBinIdx, groupHistogramBins[histogramLocalBinIdx]);
}
