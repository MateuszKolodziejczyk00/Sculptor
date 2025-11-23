#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(GenerateLightsDrawCommnadsDS, 1)]]
[[descriptor_set(DepthCullingDS, 2)]]

#include "Utils/Wave.hlsli"
#include "Utils/Culling.hlsli"
#include "Lights/LightingUtils.hlsli"


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
		const LocalLightGPUData localLight = u_localLights[lightIdx];

		float3 boundingSphereCenter;
		float boundingSphereRadius;
		CreateLightBoundingSphere(localLight, OUT boundingSphereCenter, OUT boundingSphereRadius);

		bool isLightVisible = IsSphereInFrustum(u_sceneViewCullingData.cullingPlanes, boundingSphereCenter, boundingSphereRadius);

		if(isLightVisible)
		{
			const float near = GetNearPlane(u_sceneView);
			const float p01 = u_sceneView.projectionMatrix[0][1];
			const float p12 = u_sceneView.projectionMatrix[1][2];

			const float2 hiZRes = u_depthCullingParams.hiZResolution;

			const float3 boundingSphereCenterVS = mul(u_sceneView.viewMatrix, float4(boundingSphereCenter, 1.f)).xyz;

			float4 aabbOnScreen = 0.f;
			isLightVisible = !IsSphereCenterBehindHiZ(u_hiZTexture, u_hiZSampler, hiZRes, boundingSphereCenterVS, boundingSphereRadius, near, p01, p12, OUT aabbOnScreen);
		}

		if(isLightVisible)
		{
			LightIndirectDrawCommand lightDrawCommand;
			lightDrawCommand.vertexCount    = localLight.type == LIGHT_TYPE_POINT ? u_constants.pointLightProxyVerticesNum : u_constants.spotLightProxyVerticesNum;
			lightDrawCommand.instanceCount  = 1;
			lightDrawCommand.firstVertex    = 0;
			lightDrawCommand.firstInstance  = 0;
			lightDrawCommand.localLightIdx  = lightIdx;

			if (localLight.type == LIGHT_TYPE_POINT)
			{
				const uint2 visiblePointLightsBallot = WaveActiveBallot(isLightVisible).xy;
				const uint visiblePointLightsNum = countbits(visiblePointLightsBallot.x) + countbits(visiblePointLightsBallot.y);

				uint outputPointLightIdx = 0;
				if (WaveIsFirstLane())
				{
					InterlockedAdd(u_pointLightDrawsCount[0], visiblePointLightsNum, outputPointLightIdx);
				}

				outputPointLightIdx = WaveReadLaneFirst(outputPointLightIdx) + GetCompactedIndex(visiblePointLightsBallot, WaveGetLaneIndex());

				u_pointLightDraws[outputPointLightIdx] = lightDrawCommand;
			}
			else if (localLight.type == LIGHT_TYPE_SPOT)
			{
				const uint2 visibleSpotLightsBallot = WaveActiveBallot(isLightVisible).xy;
				const uint visibleSpotLightsNum = countbits(visibleSpotLightsBallot.x) + countbits(visibleSpotLightsBallot.y);

				uint outputSpotLightIdx = 0;
				if (WaveIsFirstLane())
				{
					InterlockedAdd(u_spotLightDrawsCount[0], visibleSpotLightsNum, outputSpotLightIdx);
				}

				outputSpotLightIdx = WaveReadLaneFirst(outputSpotLightIdx) + GetCompactedIndex(visibleSpotLightsBallot, WaveGetLaneIndex());

				u_spotLightDraws[outputSpotLightIdx] = lightDrawCommand;
			}
		}
	}
}
