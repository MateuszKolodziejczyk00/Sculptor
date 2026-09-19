#include "SculptorShader.hlsli"

[[shader_params(GenerateRayDirectionsConstants, PARAMS_GENERATE_RAY_DIRECTIONS)]]
[[shader_params(GPURenderView, VIEW)]]

#include "Utils/SceneViewUtils.hlsli"

#include "SpecularReflections/RTGITracing.hlsli"

#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"
#include "Utils/VariableRate/VariableRate.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(64, 1, 1)]
void GenerateRayDirectionsCS(CS_INPUT input)
{
	const uint traceCommandIndex = input.globalID.x;

	if(traceCommandIndex >= PARAMS_GENERATE_RAY_DIRECTIONS->tracesNum[0])
	{
		return;
	}

	const EncodedRayTraceCommand encodedTraceCommand = PARAMS_GENERATE_RAY_DIRECTIONS->traceCommands[traceCommandIndex];
	const RayTraceCommand traceCommand = DecodeTraceCommand(encodedTraceCommand);

	const uint2 pixel = traceCommand.blockCoords + traceCommand.localOffset;

	const float depth = PARAMS_GENERATE_RAY_DIRECTIONS->depthTexture.Load(uint3(pixel, 0));
	if (depth > 0.f)
	{
		const float2 uv = (pixel + 0.5f) * PARAMS_GENERATE_RAY_DIRECTIONS->invResolution;
		const float3 ndc = float3(uv * 2.f - 1.f, depth);
		const float3 worldLocation = NDCToWorldSpace(ndc, VIEW->sceneView);

		const float3 normal            = OctahedronDecodeNormal(PARAMS_GENERATE_RAY_DIRECTIONS->normalsTexture.Load(uint3(pixel, 0)));
		const float  roughness         = PARAMS_GENERATE_RAY_DIRECTIONS->roughnessTexture.Load(uint3(pixel, 0));
		const float4 baseColorMetallic = PARAMS_GENERATE_RAY_DIRECTIONS->baseColorMetallicTexture.Load(uint3(pixel, 0));

		const float3 toView = normalize(VIEW->sceneView.viewLocation - worldLocation);

        RngState rng = RngState::Create(pixel, PARAMS_GENERATE_RAY_DIRECTIONS->seed);

		const RayDirectionInfo rayDirection = GenerateReflectionRayDir(baseColorMetallic, normal, roughness, toView, rng);

		PARAMS_GENERATE_RAY_DIRECTIONS->rwRaysDirections[traceCommandIndex] = PackHalf2x16Norm(OctahedronEncodeNormal(rayDirection.direction));
		PARAMS_GENERATE_RAY_DIRECTIONS->rwRaysPdfs[traceCommandIndex]       = rayDirection.pdf;
	}
}
