#include "SculptorShader.hlsli"
#include "Utils/SceneViewUtils.hlsli"

[[descriptor_set(ApplyAtmosphereDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


float3 GetLuminanceFromSkyViewLUT(float3 viewLocation, float3 rayDirection)
{
    const float viewHeight = length(viewLocation);
    const float3 upVector = viewLocation / viewHeight;

    const float distToHorizon = sqrt(Pow2(viewHeight) - Pow2(u_atmosphereParams.groundRadiusMM));
    const float horizonAngle = acos(distToHorizon / viewHeight);

    const float altitudeAngle = horizonAngle - acos(dot(upVector, rayDirection));

    const float3 rightVector = cross(upVector, FORWARD_VECTOR);
    const float3 forwardVector = cross(rightVector, upVector);

    const float3 projectedDir = normalize(rayDirection - upVector * dot(rayDirection, upVector));
    const float sinTheta = dot(rightVector, projectedDir);
    const float cosTheta = dot(forwardVector, projectedDir);

    const float azimuthAngle = atan2(sinTheta, cosTheta);
    
    const float v = 0.5f + 0.5f * sign(altitudeAngle) * sqrt(abs(altitudeAngle) * 2.f * INV_PI);
    const float2 uv = float2(0.5f + azimuthAngle * 0.5f * INV_PI, v);

    return u_skyViewLUT.SampleLevel(u_linearSampler, uv, 0);
}


[numthreads(8, 8, 1)]
void ApplyAtmosphereCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;
    
    uint2 outputRes;
    u_luminanceTexture.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 pixelSize = rcp(float2(outputRes));
        const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

        const float depth = u_depthTexture.SampleLevel(u_nearestSampler, uv, 0);

        if(depth < 0.00001f)
        {
            const float3 rayDirection = ComputeViewRayDirection(u_sceneView, uv);

            const float3 viewLocation = float3(0.f, 0.f, u_atmosphereParams.groundRadiusMM + u_atmosphereParams.viewHeight * 0.000001f);

            float3 skyLuminance = GetLuminanceFromSkyViewLUT(viewLocation, rayDirection);

            u_luminanceTexture[pixel] = skyLuminance;
        }
    }
}
