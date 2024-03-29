#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(GenerateLightsDrawCommnadsDS, 1)]]
[[descriptor_set(DepthCullingDS, 2)]]

#include "Utils/Wave.hlsli"
#include "Utils/Culling.hlsli"
#include "Lights/LightProxyVerticesInfo.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(64, 1, 1)]
void GenerateLightsDrawCommandsCS(CS_INPUT input)
{
    const uint lightIdx = input.globalID.x;
    
    if(lightIdx < u_lightsData.localLightsNum)
    {
        const PointLightGPUData pointLight = u_localLights[lightIdx];

        bool isLightVisible = IsSphereInFrustum(u_sceneViewCullingData.cullingPlanes, pointLight.location, pointLight.radius);

        if(isLightVisible)
        {
            const float3 pointLightCenterVS = mul(u_sceneView.viewMatrix, float4(pointLight.location, 1.f)).xyz;

            const float near = GetNearPlane(u_sceneView);
            const float p01 = u_sceneView.projectionMatrix[0][1];
            const float p12 = u_sceneView.projectionMatrix[1][2];

            const float2 hiZRes = u_depthCullingParams.hiZResolution;

            float4 aabbOnScreen = 0.f;
            isLightVisible = !IsSphereCenterBehindHiZ(u_hiZTexture, u_hiZSampler, hiZRes, pointLightCenterVS, pointLight.radius, near, p01, p12, aabbOnScreen);
        }
        
        if(isLightVisible)
        {
            const uint2 visibleLightsBallot = WaveActiveBallot(isLightVisible).xy;
        
            const uint visibleLightsNum = countbits(visibleLightsBallot.x) + countbits(visibleLightsBallot.y);

            uint outputBufferIdx = 0;
            if (WaveIsFirstLane())
            {
                InterlockedAdd(u_lightDrawsCount[0], visibleLightsNum, outputBufferIdx);
            }

            outputBufferIdx = WaveReadLaneFirst(outputBufferIdx) + GetCompactedIndex(visibleLightsBallot, WaveGetLaneIndex());

            LightIndirectDrawCommand lightDrawCommand;
            lightDrawCommand.vertexCount    = POINT_LIGHT_PROXY_VERTICES_NUM;
	        lightDrawCommand.instanceCount  = 1;
            lightDrawCommand.firstVertex    = 0;
            lightDrawCommand.firstInstance  = 0;
            lightDrawCommand.localLightIdx  = lightIdx;

            u_lightDraws[outputBufferIdx] = lightDrawCommand;

            u_visibleLightsReadbackBuffer[outputBufferIdx] = pointLight.entityID;
        }
    }
}
