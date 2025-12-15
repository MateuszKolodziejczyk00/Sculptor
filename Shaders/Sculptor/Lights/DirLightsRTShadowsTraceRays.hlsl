#include "SculptorShader.hlsli"

[[descriptor_set(RenderSceneDS)]]
[[descriptor_set(TraceShadowRaysDS)]]
[[descriptor_set(DirectionalLightShadowMaskDS)]]
[[descriptor_set(RenderViewDS)]]

#include "Utils/BlueNoiseSamples.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Random.hlsli"
#include "Utils/Packing.hlsli"
#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"
#include "Utils/VariableRate/VariableRate.hlsli"
#include "RayTracing/RayTracingHelpers.hlsli"


float TraceShadowRay(in uint2 pixel)
{
	const float2 uv = (pixel + 0.5f) / float2(u_params.resolution);
	const float depth = u_depthTexture.Load(uint3(pixel, 0)).r;

	float result = 0.f;

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
			
			const uint sampleIdx = (((pixel.y & 15u) * 16u + (pixel.x & 15u) + GPUScene().frameIdx * 23u)) & 255u;
			const float2 noise = frac(g_BlueNoiseSamples[sampleIdx]);
			const float3 shadowRayDirection = VectorInCone(-u_params.lightDirection, maxConeAngle, noise);

			RayDesc rayDesc;
			rayDesc.TMin        = 0.f;
			rayDesc.TMax        = u_params.maxTraceDistance;
			rayDesc.Origin      = worldLocation;
			rayDesc.Direction   = shadowRayDirection;

			result = RTScene().VisibilityTest(rayDesc) ? 1.f : 0.f;
		}
	}

	return result;
}


void OutputShadowMask(in RayTraceCommand command, in float shadowMaskValue)
{
	const uint2 outputCoords = command.blockCoords + command.localOffset;
	u_shadowMask[outputCoords] = shadowMaskValue;
}

[shader("raygeneration")]
void GenerateShadowRaysRTG()
{
	const EncodedRayTraceCommand encodedTraceCommand = u_traceCommands[DispatchRaysIndex().x];
	const RayTraceCommand traceCommand = DecodeTraceCommand(encodedTraceCommand);

	const uint2 coords = traceCommand.blockCoords + traceCommand.localOffset;

	const float shadowMaskValue = TraceShadowRay(coords);
	OutputShadowMask(traceCommand, shadowMaskValue);
}
