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
    
    uint2 outputRes;
    u_LDRTexture.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float EV100 = ComputeEV100FromAvgLuminance(u_adaptedLuminance[0] + 0.0001);
        const float exposure = ConvertEV100ToExposure(EV100);

        const float2 pixelSize = rcp(float2(outputRes));
  
        const float2 uv = (float2(pixel) + 0.5f) * pixelSize;
        float3 color = u_radianceTexture.SampleLevel(u_sampler, uv, 0).xyz;
        color *= exposure;

        color = float3(GTTonemapper(color.r), GTTonemapper(color.g), GTTonemapper(color.b));

        color = LinearTosRGB(color);

        if(u_tonemappingSettings.enableColorDithering)
        {
            color += Random(pixel) * rcp(255.f);
        }

        u_LDRTexture[pixel] = float4(color, 1.f);
    }
}