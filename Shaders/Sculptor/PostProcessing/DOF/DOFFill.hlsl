#include "SculptorShader.hlsli"

[[descriptor_set(DOFFillPassDS, 0)]]

#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void DOFFillCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;
    
    uint2 outputRes;
    u_nearFieldFilledDOFTexture.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 pixelSize = 1.0f / float2(outputRes);
        const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

        const float cocFar  = u_cocTexture.SampleLevel(u_nearestSampler, uv, 0).y;
        const float cocNear = u_cocNearBlurredTexture.SampleLevel(u_nearestSampler, uv, 0);

        float3 dofNear = u_nearFieldDOFTexture.SampleLevel(u_nearestSampler, uv, 0);

        if(cocNear > 0.f)
        {
            [unroll]
            for (int y = -1; y <= 1; ++y)
            {
                [unroll]
                for (int x = -1; x <= 1; ++x)
                {
                    const float2 sampleUV = uv + float2(x, y) * pixelSize;
                    dofNear = max(dofNear, u_nearFieldDOFTexture.SampleLevel(u_nearestSampler, sampleUV, 0));
                }
            }
        }

        float3 dofFar = u_farFieldDOFTexture.SampleLevel(u_nearestSampler, uv, 0);

        if(cocFar > 0.f)
        {
            [unroll]
            for (int y = -1; y <= 1; ++y)
            {
                [unroll]
                for (int x = -1; x <= 1; ++x)
                {
                    const float2 sampleUV = uv + float2(x, y) * pixelSize;
                    dofFar = max(dofFar, u_farFieldDOFTexture.SampleLevel(u_nearestSampler, sampleUV, 0));
                }
            }
        }
	
        u_nearFieldFilledDOFTexture[pixel] = dofNear;
        u_farFieldFilledDOFTexture[pixel] = dofFar;
    }
}
