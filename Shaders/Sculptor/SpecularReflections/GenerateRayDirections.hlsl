#include "SculptorShader.hlsli"

[[descriptor_set(GenerateRayDirectionsDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

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

	if(traceCommandIndex >= u_tracesNum[0])
	{
		return;
	}

	const EncodedRayTraceCommand encodedTraceCommand = u_traceCommands[traceCommandIndex];
	const RayTraceCommand traceCommand = DecodeTraceCommand(encodedTraceCommand);

	const uint2 pixel = traceCommand.blockCoords + traceCommand.localOffset;

	const float depth = u_depthTexture.Load(uint3(pixel, 0));
	if (depth > 0.f)
	{
		const float2 uv = (pixel + 0.5f) * u_constants.invResolution;
		const float3 ndc = float3(uv * 2.f - 1.f, depth);
		const float3 worldLocation = NDCToWorldSpace(ndc, u_sceneView);

		const float3 normal            = OctahedronDecodeNormal(u_normalsTexture.Load(uint3(pixel, 0)));
		const float  roughness         = u_roughnessTexture.Load(uint3(pixel, 0));
		const float4 baseColorMetallic = u_baseColorMetallicTexture.Load(uint3(pixel, 0));

		const float3 toView = normalize(u_sceneView.viewLocation - worldLocation);

        RngState rng = RngState::Create(pixel, u_constants.seed);

		const RayDirectionInfo rayDirection = GenerateReflectionRayDir(baseColorMetallic, normal, roughness, toView, rng);

		u_rwRaysDirections[traceCommandIndex] = PackHalf2x16Norm(OctahedronEncodeNormal(rayDirection.direction));
		u_rwRaysPdfs[traceCommandIndex]       = rayDirection.pdf;
	}
}
