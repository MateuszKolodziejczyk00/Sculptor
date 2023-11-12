#include "SculptorShader.hlsli"
#include "Utils/SceneViewUtils.hlsli"

[[descriptor_set(DepthBasedUpsampleDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


static const float depthDiffThreshold = 0.035f;


float ComputeSampleWeight(float sampleDepthDiff)
{
    return sampleDepthDiff <= depthDiffThreshold ? rcp(Pow2(sampleDepthDiff / depthDiffThreshold + 1.f)) : 0.f;
}


[numthreads(8, 8, 1)]
void DepthBasedUpsampleCS(CS_INPUT input)
{
    const int2 pixel = input.globalID.xy;
    
    uint2 outputRes;
    u_depthTexture.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 outputPixelSize = rcp(float2(outputRes));
        const float2 outputUV = (float2(pixel) + 0.5f) * outputPixelSize;
        
        uint2 inputRes;
        u_depthTextureHalfRes.GetDimensions(inputRes.x, inputRes.y);
        const float2 inputPixelSize = rcp(float2(inputRes));

        const int2 inputPixel = pixel / 2 + (pixel & 1) - 1;
        const float2 inputUV = float2(inputPixel + 0.5f) * inputPixelSize;

        const float2 uv0 = inputUV + float2(0.f, 1.f) * inputPixelSize;
        const float2 uv1 = inputUV + float2(1.f, 1.f) * inputPixelSize;
        const float2 uv2 = inputUV + float2(1.f, 0.f) * inputPixelSize;
        const float2 uv3 = inputUV + float2(0.f, 0.f) * inputPixelSize;

        const float4 inputDepths = u_depthTextureHalfRes.Gather(u_nearestSampler, inputUV, 0);
        const float4 inputLinearDepths = ComputeLinearDepth(inputDepths, u_sceneView);

        const float4 input0 = u_inputTexture.SampleLevel(u_nearestSampler, uv0, 0);
        const float4 input1 = u_inputTexture.SampleLevel(u_nearestSampler, uv1, 0);
        const float4 input2 = u_inputTexture.SampleLevel(u_nearestSampler, uv2, 0);
        const float4 input3 = u_inputTexture.SampleLevel(u_nearestSampler, uv3, 0);

        const float outputDepth = u_depthTexture.SampleLevel(u_nearestSampler, outputUV, 0).x;
        const float outputLinearDepth = ComputeLinearDepth(outputDepth, u_sceneView);

        const float4 depthDiff = abs(inputLinearDepths - outputLinearDepth);
        const float minDepthDiff = min(min(depthDiff.x, depthDiff.y), min(depthDiff.z, depthDiff.w));

        float weight0 = ComputeSampleWeight(abs(depthDiff.x - minDepthDiff));
        float weight1 = ComputeSampleWeight(abs(depthDiff.y - minDepthDiff));
        float weight2 = ComputeSampleWeight(abs(depthDiff.z - minDepthDiff));
        float weight3 = ComputeSampleWeight(abs(depthDiff.w - minDepthDiff));

        if(u_params.eliminateFireflies)
        {
            weight0 /= (1.f + Luminance(input0.rgb));
            weight1 /= (1.f + Luminance(input1.rgb));
            weight2 /= (1.f + Luminance(input2.rgb));
            weight3 /= (1.f + Luminance(input3.rgb));
        }

        const float rcpWeightSum = rcp(weight0 + weight1 + weight2 + weight3);

        const float4 output = (input0 * weight0 + input1 * weight1 + input2 * weight2 + input3 * weight3) * rcpWeightSum;

        u_outputTexture[pixel] = output;
    }
}
