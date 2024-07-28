#include "SculptorShader.hlsli"

[[descriptor_set(TraceShadowRaysDS, 2)]]
[[descriptor_set(DirectionalLightShadowMaskDS, 3)]]
[[descriptor_set(RenderViewDS, 4)]]

#include "Utils/BlueNoiseSamples.hlsli"
#include "Utils/RTVisibilityCommon.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Random.hlsli"
#include "Utils/Packing.hlsli"
#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"
#include "Utils/VariableRate/VariableRate.hlsli"


float TraceShadowRay(in uint2 pixel)
{
	const float2 uv = (pixel + 0.5f) / float2(u_params.resolution);
	const float depth = u_depthTexture.Load(uint3(pixel, 0)).r;

	RTVisibilityPayload payload = { false };

	if(depth > 0.f)
	{
		const float3 ndc = float3(uv * 2.f - 1.f, depth);
		float3 worldLocation = NDCToWorldSpace(ndc, u_sceneView);

		const float3 normal = OctahedronDecodeNormal(u_normalsTexture.Load(uint3(pixel, 0)));

		if(dot(normal, u_params.lightDirection) <= 0.015f)
		{
			const float3 bias = normalize(u_sceneView.viewLocation - worldLocation) * u_params.shadowRayBias;
			worldLocation += bias;

			const float maxConeAngle = u_params.shadowRayConeAngle;
			
			const uint sampleIdx = (((pixel.y & 15u) * 16u + (pixel.x & 15u) + u_gpuSceneFrameConstants.frameIdx * 23u)) & 255u;
			const float2 noise = frac(g_BlueNoiseSamples[sampleIdx]);
			const float3 shadowRayDirection = VectorInCone(-u_params.lightDirection, maxConeAngle, noise);

			RayDesc rayDesc;
			rayDesc.TMin        = u_params.minTraceDistance;
			rayDesc.TMax        = u_params.maxTraceDistance;
			rayDesc.Origin      = worldLocation;
			rayDesc.Direction   = shadowRayDirection;

			TraceRay(u_sceneTLAS,
					 RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
					 0xFF,
					 0,
					 1,
					 0,
					 rayDesc,
					 payload);
		}
		else
		{
			payload.isVisible = false;
		}
	}

	return payload.isVisible ? 1.f : 0.f;
}


void OutputShadowMask(in RayTraceCommand command, in float shadowMaskValue)
{
	const uint2 outputCoords = command.blockCoords + command.localOffset;
	u_shadowMask[outputCoords] = shadowMaskValue;
}


void OutputVRBlockInfo(in RayTraceCommand command)
{
	const uint2 blockSize = GetVariableRateTileSize(command.variableRateMask);

	const uint blockInfo = PackVRBlockInfo(command.localOffset, command.variableRateMask);

	for(uint y = 0; y < blockSize.y; ++y)
	{
		for(uint x = 0; x < blockSize.x; ++x)
		{
			const uint2 outputCoords = command.blockCoords + uint2(x, y);
			u_vrBlocksTexture[outputCoords] = blockInfo;
		}
	}
}


[shader("raygeneration")]
void GenerateShadowRaysRTG()
{
	const EncodedRayTraceCommand encodedTraceCommand = u_traceCommands[DispatchRaysIndex().x];
	const RayTraceCommand traceCommand = DecodeTraceCommand(encodedTraceCommand);

	const uint2 pixel = traceCommand.blockCoords + traceCommand.localOffset;

	const float shadowMaskValue = TraceShadowRay(pixel);

	OutputShadowMask(traceCommand, shadowMaskValue);

	OutputVRBlockInfo(traceCommand);
}
