#include "SculptorShader.hlsli"
#include "Utils/Exposure.hlsli"

[[descriptor_set(BloomPassDS, 0)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


float3 DownsampleFilter(Texture2D inputTexture, SamplerState inputSampler, float2 uv, float2 pixelSize)
{
    const float3 Center = inputTexture.SampleLevel(inputSampler, uv, 0).xyz;

    const float3 Inner0 = inputTexture.SampleLevel(inputSampler, uv + pixelSize * float2(-1.f, -1.f), 0).xyz;
    const float3 Inner1 = inputTexture.SampleLevel(inputSampler, uv + pixelSize * float2(-1.f,  1.f), 0).xyz;
    const float3 Inner2 = inputTexture.SampleLevel(inputSampler, uv + pixelSize * float2( 1.f, -1.f), 0).xyz;
    const float3 Inner3 = inputTexture.SampleLevel(inputSampler, uv + pixelSize * float2( 1.f,  1.f), 0).xyz;

    const float3 Outer0 = inputTexture.SampleLevel(inputSampler, uv + pixelSize * float2(-2.f, -2.f), 0).xyz;
    const float3 Outer1 = inputTexture.SampleLevel(inputSampler, uv + pixelSize * float2(-2.f,  0.f), 0).xyz;
    const float3 Outer2 = inputTexture.SampleLevel(inputSampler, uv + pixelSize * float2(-2.f,  2.f), 0).xyz;
    const float3 Outer3 = inputTexture.SampleLevel(inputSampler, uv + pixelSize * float2( 0.f, -2.f), 0).xyz;
    const float3 Outer4 = inputTexture.SampleLevel(inputSampler, uv + pixelSize * float2( 0.f,  2.f), 0).xyz;
    const float3 Outer5 = inputTexture.SampleLevel(inputSampler, uv + pixelSize * float2( 2.f, -2.f), 0).xyz;
    const float3 Outer6 = inputTexture.SampleLevel(inputSampler, uv + pixelSize * float2( 2.f,  0.f), 0).xyz;
    const float3 Outer7 = inputTexture.SampleLevel(inputSampler, uv + pixelSize * float2( 2.f,  2.f), 0).xyz;

    // Outer + Center layout
    // 2 4 7
    // 1 C 6
    // 0 3 5

    float3 result = (Inner0 + Inner1 + Inner2 + Inner3) * 0.5f;
    result += (Outer0 + Outer3 + Outer1 + Center) * 0.125f;
    result += (Outer1 + Outer2 + Outer4 + Center) * 0.125f;
    result += (Outer4 + Outer6 + Outer7 + Center) * 0.125f;
    result += (Outer3 + Outer5 + Outer6 + Center) * 0.125f;
    result *= 0.25f;

    return result;
}


float3 UpsampleFilter(Texture2D inputTexture, SamplerState inputSampler, float2 uv, float2 pixelSize)
{
    float3 result = 0.f;
    
    result += inputTexture.SampleLevel(inputSampler, uv, 0).xyz * 4.f;
    result += inputTexture.SampleLevel(inputSampler, uv + pixelSize * float2(-1.f, -1.f), 0).xyz;
    result += inputTexture.SampleLevel(inputSampler, uv + pixelSize * float2(-1.f,  0.f), 0).xyz * 2.f;
    result += inputTexture.SampleLevel(inputSampler, uv + pixelSize * float2(-1.f,  1.f), 0).xyz;
    result += inputTexture.SampleLevel(inputSampler, uv + pixelSize * float2( 0.f, -1.f), 0).xyz * 2.f;
    result += inputTexture.SampleLevel(inputSampler, uv + pixelSize * float2( 0.f,  1.f), 0).xyz * 2.f;
    result += inputTexture.SampleLevel(inputSampler, uv + pixelSize * float2( 1.f, -1.f), 0).xyz;
    result += inputTexture.SampleLevel(inputSampler, uv + pixelSize * float2( 1.f,  0.f), 0).xyz * 2.f;
    result += inputTexture.SampleLevel(inputSampler, uv + pixelSize * float2( 1.f,  1.f), 0).xyz;

    return result * (1.f / 16.f);
}


[numthreads(8, 8, 1)]
void BloomDownsampleCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;

    uint2 outputRes;
    u_outputTexture.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 inputPixelSize = u_bloomInfo.inputPixelSize;
        const float2 outputPixelSize = u_bloomInfo.outputPixelSize;

        const float2 uv = pixel * outputPixelSize + outputPixelSize * 0.5f;
        
        float3 input = DownsampleFilter(u_inputTexture, u_inputSampler, uv, inputPixelSize);
        
        if(u_bloomInfo.prefilterInput)
        {
            const float EV100 = ComputeEV100FromAvgLuminance(u_adaptedLuminance[0] + 0.0001);

            const float bloomEV = EV100 + u_bloomInfo.bloomEC;

            input = max(input - u_bloomInfo.thresholdValue, 0.f);
            input = input * exp2(bloomEV - 3.f);
        }
       
        u_outputTexture[pixel] = float4(input, 1.f);
    }
}


[numthreads(8, 8, 1)]
void BloomUpsampleCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;

    uint2 outputRes;
    u_outputTexture.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 inputPixelSize = u_bloomInfo.inputPixelSize;
        const float2 outputPixelSize = u_bloomInfo.outputPixelSize;

        const float2 uv = pixel * outputPixelSize + outputPixelSize * 0.5f;
        
        const float3 input = UpsampleFilter(u_inputTexture, u_inputSampler, uv, inputPixelSize);

        const float3 existing = u_outputTexture[pixel].xyz;

        u_outputTexture[pixel] = float4(existing + input, 1.f);
    }
}
