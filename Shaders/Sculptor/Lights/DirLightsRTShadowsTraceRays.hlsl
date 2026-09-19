#include "SculptorShader.hlsli"

[[shader_params(RenderSceneConstants, SCENE)]]
[[shader_params(TraceShadowRaysParams, PARAMS_TRACE_SHADOW_RAYS)]]
[[shader_params(DirectionalLightShadowUpdateParams, PARAMS_DIRECTIONAL_LIGHT_SHADOW_MASK)]]
[[shader_params(GPURenderView, VIEW)]]

#include "Utils/BlueNoiseSamples.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Random.hlsli"
#include "Utils/Packing.hlsli"
#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"
#include "Utils/VariableRate/VariableRate.hlsli"
#include "RayTracing/RayTracingHelpers.hlsli"


float TraceShadowRay(in uint2 pixel)
{
	const float2 uv = (pixel + 0.5f) / float2(PARAMS_DIRECTIONAL_LIGHT_SHADOW_MASK->resolution);
	const float depth = PARAMS_TRACE_SHADOW_RAYS->depthTexture.Load(uint3(pixel, 0)).r;

	float result = 0.f;

	if(depth > 0.f)
	{
		const float3 ndc = float3(uv * 2.f - 1.f, depth);
		float3 worldLocation = NDCToWorldSpace(ndc, VIEW->sceneView);

#if !CONTINUE_RAYS
		const float3 normal = OctahedronDecodeNormal(PARAMS_TRACE_SHADOW_RAYS->normalsTexture.Load(uint3(pixel, 0)));

		if(dot(normal, PARAMS_DIRECTIONAL_LIGHT_SHADOW_MASK->lightDirection) <= 0.015f)
#endif // !CONTINUE_RAYS
		{
#if !CONTINUE_RAYS
			const float3 bias = normalize(VIEW->sceneView.viewLocation - worldLocation) * PARAMS_DIRECTIONAL_LIGHT_SHADOW_MASK->shadowRayBias;
			worldLocation += bias;
#endif // !CONTINUE_RAYS

			const float maxConeAngle = PARAMS_DIRECTIONAL_LIGHT_SHADOW_MASK->shadowRayConeAngle;
			
			const uint sampleIdx = (((pixel.y & 15u) * 16u + (pixel.x & 15u) + GPUScene().frameIdx * 23u)) & 255u;
			const float2 noise = frac(g_BlueNoiseSamples[sampleIdx]);
			const float3 shadowRayDirection = VectorInCone(-PARAMS_DIRECTIONAL_LIGHT_SHADOW_MASK->lightDirection, maxConeAngle, noise);

			float minT = 0.f;

#if CONTINUE_RAYS
			const uint16_t continuationDist = PARAMS_DIRECTIONAL_LIGHT_SHADOW_MASK->rayContinuationDists.Load(DispatchRaysIndex().x);
			minT = (continuationDist / 255.f) * PARAMS_DIRECTIONAL_LIGHT_SHADOW_MASK->ssTraceDistance;
#endif // CONTINUE_RAYS

			RayDesc rayDesc;
			rayDesc.TMin        = minT;
			rayDesc.TMax        = PARAMS_DIRECTIONAL_LIGHT_SHADOW_MASK->maxTraceDistance;
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
	PARAMS_DIRECTIONAL_LIGHT_SHADOW_MASK->shadowMask[outputCoords] = shadowMaskValue;
}

[shader("raygeneration")]
void GenerateShadowRaysRTG()
{
	const EncodedRayTraceCommand encodedTraceCommand = PARAMS_TRACE_SHADOW_RAYS->traceCommands[DispatchRaysIndex().x];
	const RayTraceCommand traceCommand = DecodeTraceCommand(encodedTraceCommand);

	const uint2 coords = traceCommand.blockCoords + traceCommand.localOffset;

	const float shadowMaskValue = TraceShadowRay(coords);
	OutputShadowMask(traceCommand, shadowMaskValue);
}
