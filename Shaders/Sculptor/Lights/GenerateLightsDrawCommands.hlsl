#include "SculptorShader.hlsli"
#include "Utils/Wave.hlsli"
#include "Utils/Culling.hlsli"
#include "Lights/LightProxyVerticesInfo.hlsli"


[[descriptor_set(GenerateLightsDrawCommnadsDS, 0)]]


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

        const bool isLightInFrustum = IsSphereInFrustum(u_sceneViewCullingData.cullingPlanes, pointLight.location, pointLight.radius);
        
        if(isLightInFrustum)
        {
            const uint2 visibleLightsBallot = WaveActiveBallot(isLightInFrustum).xy;
        
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
        }
    }
}
