#include "SculptorShader.hlsli"
#include "Utils/SceneViewUtils.hlsli"

[[descriptor_set(GenerateGeometryNormalsDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};

// Based on https://atyuwen.github.io/posts/normal-reconstruction/


[numthreads(8, 8, 1)]
void GenerateGeometryNormalsCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;
    
    uint2 outputRes;
    u_geometryNormalsTexture.GetDimensions(outputRes.x, outputRes.y);

    if (all(pixel < outputRes))
    {
        const float2 pixelSize = rcp(float2(outputRes));
        const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

        const float sampleDepth = u_depthTexture.SampleLevel(u_nearestSampler, uv, 0.f).x;

        float4 horizontalDepth;
        horizontalDepth.x = u_depthTexture.SampleLevel(u_nearestSampler, uv + float2(-1.f, 0.f) * pixelSize, 0.f).x;
        horizontalDepth.y = u_depthTexture.SampleLevel(u_nearestSampler, uv + float2( 1.f, 0.f) * pixelSize, 0.f).x;
        horizontalDepth.z = u_depthTexture.SampleLevel(u_nearestSampler, uv + float2(-2.f, 0.f) * pixelSize, 0.f).x;
        horizontalDepth.w = u_depthTexture.SampleLevel(u_nearestSampler, uv + float2( 2.f, 0.f) * pixelSize, 0.f).x;

        float4 verticalDepth;
        verticalDepth.x = u_depthTexture.SampleLevel(u_nearestSampler, uv + float2(0.f, -1.f * pixelSize.y), 0.f).x;
        verticalDepth.y = u_depthTexture.SampleLevel(u_nearestSampler, uv + float2(0.f,  1.f * pixelSize.y), 0.f).x;
        verticalDepth.z = u_depthTexture.SampleLevel(u_nearestSampler, uv + float2(0.f, -2.f * pixelSize.y), 0.f).x;
        verticalDepth.w = u_depthTexture.SampleLevel(u_nearestSampler, uv + float2(0.f,  2.f * pixelSize.y), 0.f).x;

        const float3 sampleVL = NDCToViewSpace(float3(uv * 2.f - 1.f, sampleDepth), u_sceneView.inverseProjection);

        const float2 downUV = uv + float2(0.f, -1.f * pixelSize.y);
        const float3 downSampleVL = NDCToViewSpace(float3(downUV * 2.f - 1.f, verticalDepth.x), u_sceneView.inverseProjection);

        const float2 upUV = uv + float2(0.f, 1.f * pixelSize.y);
        const float3 upSampleVL = NDCToViewSpace(float3(upUV * 2.f - 1.f, verticalDepth.y), u_sceneView.inverseProjection);

        const float2 leftUV = uv + float2(-1.f * pixelSize.x, 0.f);
        const float3 leftSampleVL = NDCToViewSpace(float3(leftUV * 2.f - 1.f, horizontalDepth.x), u_sceneView.inverseProjection);

        const float2 rightUV = uv + float2(1.f * pixelSize.x, 0.f);
        const float3 rightSampleVL = NDCToViewSpace(float3(rightUV * 2.f - 1.f, horizontalDepth.y), u_sceneView.inverseProjection);

        const float2 he = abs((2.f * horizontalDepth.xy - horizontalDepth.zw) - sampleDepth);
        const float2 ve = abs((2.f * verticalDepth.xy - verticalDepth.zw) - sampleDepth);

        float3 hDeriv;
        if(pixel.x == 0)
        {
            hDeriv = rightSampleVL - sampleVL;
        }
        else if(pixel.x == outputRes.x - 1)
        {
            hDeriv = leftSampleVL - sampleVL;
        }
        else
        {
            hDeriv = he.x < he.y ? leftSampleVL - sampleVL : rightSampleVL - sampleVL;
        }

        float3 vDeriv;
        if(pixel.y == 0)
        {
            vDeriv = upSampleVL - sampleVL;
        }
        else if(pixel.y == outputRes.y - 1)
        {
            vDeriv = downSampleVL - sampleVL;
        }
        else
        {
            vDeriv = ve.x < ve.y ? downSampleVL - sampleVL : upSampleVL - sampleVL;
        }

        float3 normalViewSpace = normalize(cross(hDeriv, vDeriv));
        if (dot(normalViewSpace, sampleVL) > 0.f)
        {
            normalViewSpace *= -1.f;
        }
        
        const float3 normalWorldSpace = mul(u_sceneView.inverseView, float4(normalViewSpace, 0.f)).xyz;

        const float3 textureNormal = (normalWorldSpace + 1.f) * 0.5f;

        u_geometryNormalsTexture[pixel] = float4(textureNormal, 1.f);
    }
}
