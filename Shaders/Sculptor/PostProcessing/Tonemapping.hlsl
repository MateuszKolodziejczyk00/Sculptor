#include "SculptorShader.hlsli"
#include "Utils/Exposure.hlsli"
#include "Utils/TonemappingOperators.hlsli"

[[descriptor_set(TonemappingDS, 0)]]


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

        color = LinearTosRGB(TonemapACES(color));

        u_LDRTexture[pixel] = float4(color, 1.f);
    }
}