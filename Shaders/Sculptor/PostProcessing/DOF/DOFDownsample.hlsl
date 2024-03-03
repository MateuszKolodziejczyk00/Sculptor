#include "SculptorShader.hlsli"

[[descriptor_set(DOFDownsampleDS, 0)]]

#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void DOFDownsampleCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;
    
    uint2 outputRes;
    u_cocHalfTexture.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 inputPixel = pixel * 2.f;
        const float2 uv00 = inputPixel / float2(u_params.inputResolution);
        const float2 uv01 = inputPixel / float2(u_params.inputResolution) + float2(0.f, u_params.inputPixelSize.y);
        const float2 uv10 = inputPixel / float2(u_params.inputResolution) + float2(u_params.inputPixelSize.x, 0.f);
        const float2 uv11 = inputPixel / float2(u_params.inputResolution) + u_params.inputPixelSize;

        const float2 coc = u_cocTexture.SampleLevel(u_nearestSampler, uv00, 0).rg;

        const float cocFar00 = coc.y;
        const float cocFar01 = u_cocTexture.SampleLevel(u_nearestSampler, uv01, 0).y;
        const float cocFar10 = u_cocTexture.SampleLevel(u_nearestSampler, uv10, 0).y;
        const float cocFar11 = u_cocTexture.SampleLevel(u_nearestSampler, uv11, 0).y;
        
        const float weight00 = 1000.f;
        float3 colorMulCoCFar = weight00 * u_linearColorTexture.SampleLevel(u_nearestSampler, uv00, 0).rgb;
        float weightSum = weight00;

        const float weight01 = rcp(abs(cocFar00 - cocFar01) + 0.001f);
        colorMulCoCFar += weight01 * u_linearColorTexture.SampleLevel(u_nearestSampler, uv01, 0).rgb;
        weightSum += weight01;

        const float weight10 = rcp(abs(cocFar00 - cocFar10) + 0.001f);
        colorMulCoCFar += weight10 * u_linearColorTexture.SampleLevel(u_nearestSampler, uv10, 0).rgb;
        weightSum += weight10;

        const float weight11 = rcp(abs(cocFar00 - cocFar11) + 0.001f);
        colorMulCoCFar += weight11 * u_linearColorTexture.SampleLevel(u_nearestSampler, uv11, 0).rgb;
        weightSum += weight11;

        colorMulCoCFar /= weightSum;
        const float3 color = colorMulCoCFar;
        colorMulCoCFar *= coc.y;

        u_cocHalfTexture[pixel]                 = coc;
	    u_linearColorHalfTexture[pixel]         = color;
	    u_linearColorMulFarHalfTexture[pixel]   = colorMulCoCFar;
    }
}
