#include "SculptorShader.hlsli"
#include "Utils/SceneViewUtils.hlsli"


[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(ShadowsBilateralBlurDS, 1)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
    uint3 groupID : SV_GroupID;
    uint3 localID : SV_GroupThreadID;
};


static const float kernel[7] = { 0.0060, 0.0606, 0.2417, 0.3829, 0.2417, 0.0606, 0.0060 };


[numthreads(8, 8, 1)]
void ShadowsBilateralBlurCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;

    uint2 outputRes;
    u_outputTexture.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 pixelSize = rcp(float2(outputRes));

        const float2 uv = pixel * pixelSize + pixelSize * 0.5f;
        
        const float2 direction = u_params.isHorizontal ? float2(pixelSize.x, 0) : float2(0, pixelSize.y);

        const float nearPlane = GetNearPlane(u_sceneView.projectionMatrix);

        const float sampleDepth = u_depth.SampleLevel(u_depthSampler, uv, 0);
        const float sampleLinearDepth = ComputeLinearDepth(sampleDepth, nearPlane);

        float shadowSum = 0.f;
        float samplesNum = 0.f;

        for (int i = -3; i <= 3; ++i)
        {
            const float2 inputUV = uv + direction * i;

            const float inputDepth = u_depth.SampleLevel(u_depthSampler, inputUV, 0);
            const float inputLinearDepth = ComputeLinearDepth(inputDepth, nearPlane);
               
            if(abs(inputLinearDepth - sampleLinearDepth) <= 0.05f)
            {
                const float weight = kernel[i + 3];
                shadowSum += u_inputTexture.SampleLevel(u_inputSampler, inputUV, 0) * weight;
                samplesNum += weight;
            }
        }

        u_outputTexture[pixel] = (shadowSum / samplesNum);
    }
}