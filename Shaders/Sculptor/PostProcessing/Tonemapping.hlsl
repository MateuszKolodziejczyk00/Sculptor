#include "SculptorShader.hlsli"
#include "Utils/Exposure.hlsli"
#include "Utils/TonemappingOperators.hlsli"

[[descriptor_set(TonemappingDS, 0)]]


// Source: https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab

float3 LinearTosRGB(in float3 color)
{
    float3 x = color * 12.92f;
    float3 y = 1.055f * pow(saturate(color), 1.0f / 2.4f) - 0.055f;

    float3 clr = color;
    clr.r = color.r < 0.0031308f ? x.r : y.r;
    clr.g = color.g < 0.0031308f ? x.g : y.g;
    clr.b = color.b < 0.0031308f ? x.b : y.b;

    return clr;
}

// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
static const float3x3 ACESInputMat =
{
    {0.59719, 0.35458, 0.04823},
    {0.07600, 0.90834, 0.01566},
    {0.02840, 0.13383, 0.83777}
};

// ODT_SAT => XYZ => D60_2_D65 => sRGB
static const float3x3 ACESOutputMat =
{
    { 1.60475, -0.53108, -0.07367},
    {-0.10208,  1.10813, -0.00605},
    {-0.00327, -0.07276,  1.07602}
};

float3 RRTAndODTFit(float3 v)
{
    float3 a = v * (v + 0.0245786f) - 0.000090537f;
    float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

float3 ACESFitted(float3 color)
{
    //color = mul(ACESInputMat, color);
    color = mul(ACESInputMat, color);

    // Apply RRT and ODT
    color = RRTAndODTFit(color);

    color = mul(ACESOutputMat, color);

    // Clamp to [0, 1]
    color = saturate(color);

    return color;
}


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void TonemappingCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;

    if(pixel.x < u_tonemappingSettings.textureSize.x && pixel.y < u_tonemappingSettings.textureSize.y)
    {
        const float EV100 = ComputeEV100FromAvgLuminance(u_adaptedLuminance[0] + 0.0001);
        const float exposure = ConvertEV100ToExposure(EV100);
  
        const float2 uv = pixel * u_tonemappingSettings.inputPixelSize + u_tonemappingSettings.inputPixelSize * 0.5f;
        float3 color = u_radianceTexture.SampleLevel(u_sampler, uv, 0).xyz;
        color *= exposure;

        //color = LinearTosRGB(ACESFitted(color));
        color = LinearTosRGB(TonemapACES(color));

        u_LDRTexture[pixel] = float4(color, 1.f);
    }
}