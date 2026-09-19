#include "SculptorShader.hlsli"

[[shader_params(GPURenderView, VIEW)]]
[[shader_params(GenerateLightDrawCommandsConstants, PARAMS_GENERATE_LIGHTS_DRAW_COMMNADS)]]
[[shader_params(DepthCullingData, PARAMS_DEPTH_CULLING)]]

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

	if(lightIdx < PARAMS_GENERATE_LIGHTS_DRAW_COMMNADS->lightsData->localLightsNum)
	{
		const LocalLightInterface localLight = PARAMS_GENERATE_LIGHTS_DRAW_COMMNADS->localLights[lightIdx];

		float3 boundingSphereCenter;
		float boundingSphereRadius;
		CreateLightBoundingSphere(localLight, OUT boundingSphereCenter, OUT boundingSphereRadius);

		bool isLightVisible = IsSphereInFrustum(VIEW->cullingData.cullingPlanes, boundingSphereCenter, boundingSphereRadius);

		if(isLightVisible)
		{
			const float near = GetNearPlane(VIEW->sceneView);
			const float p01 = VIEW->sceneView.projectionMatrix[0][1];
			const float p12 = VIEW->sceneView.projectionMatrix[1][2];

			const float2 hiZRes = PARAMS_DEPTH_CULLING->hiZResolution;

			const float3 boundingSphereCenterVS = mul(VIEW->sceneView.viewMatrix, float4(boundingSphereCenter, 1.f)).xyz;

			float4 aabbOnScreen = 0.f;
			isLightVisible = !IsSphereCenterBehindHiZ(PARAMS_DEPTH_CULLING->hiZTexture, BindlessSamplers::LinearMinClampEdge(), hiZRes, boundingSphereCenterVS, boundingSphereRadius, near, p01, p12, OUT aabbOnScreen);
		}

		if(isLightVisible)
		{
			LightIndirectDrawCommand lightDrawCommand;
			lightDrawCommand.vertexCount    = localLight.type == LIGHT_TYPE_POINT ? PARAMS_GENERATE_LIGHTS_DRAW_COMMNADS->pointLightProxyVerticesNum : PARAMS_GENERATE_LIGHTS_DRAW_COMMNADS->spotLightProxyVerticesNum;
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
					outputPointLightIdx = PARAMS_GENERATE_LIGHTS_DRAW_COMMNADS->pointLightDrawsCount.AtomicAdd(0u, visiblePointLightsNum);
				}

				outputPointLightIdx = WaveReadLaneFirst(outputPointLightIdx) + GetCompactedIndex(visiblePointLightsBallot, WaveGetLaneIndex());

				PARAMS_GENERATE_LIGHTS_DRAW_COMMNADS->pointLightDraws[outputPointLightIdx] = lightDrawCommand;
			}
			else if (localLight.type == LIGHT_TYPE_SPOT)
			{
				const uint2 visibleSpotLightsBallot = WaveActiveBallot(isLightVisible).xy;
				const uint visibleSpotLightsNum = countbits(visibleSpotLightsBallot.x) + countbits(visibleSpotLightsBallot.y);

				uint outputSpotLightIdx = 0;
				if (WaveIsFirstLane())
				{
					outputSpotLightIdx = PARAMS_GENERATE_LIGHTS_DRAW_COMMNADS->spotLightDrawsCount.AtomicAdd(0u, visibleSpotLightsNum);
				}

				outputSpotLightIdx = WaveReadLaneFirst(outputSpotLightIdx) + GetCompactedIndex(visibleSpotLightsBallot, WaveGetLaneIndex());

				PARAMS_GENERATE_LIGHTS_DRAW_COMMNADS->spotLightDraws[outputSpotLightIdx] = lightDrawCommand;
			}
		}
	}
}
