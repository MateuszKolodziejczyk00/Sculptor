#include "SculptorShader.hlsli"

[[shader_params(GPURenderView, VIEW)]]
[[shader_params(GenerateRayDirectionsConstants, CONSTS)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/GBuffer/GBuffer.hlsli"

#include "SpecularReflections/RTGITracingUtils.hlsli"

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

	if(traceCommandIndex >= CONSTS->tracesNum[0])
	{
		return;
	}

	const EncodedRayTraceCommand encodedTraceCommand = CONSTS->traceCommands[traceCommandIndex];
	const RayTraceCommand traceCommand = DecodeTraceCommand(encodedTraceCommand);

	const uint2 pixel = traceCommand.blockCoords + traceCommand.localOffset;

	const SurfaceInfo surfaceInfo = CONSTS->gpuGBuffer.GetSurfaceInfo(pixel);

	if (surfaceInfo.depth > 0.f)
	{
		const float3 toView = normalize(VIEW->sceneView.viewLocation - surfaceInfo.location);

		RngState rng = RngState::Create(pixel, CONSTS->seed);

		const ShadingSample sample = RTClosure::Sample(surfaceInfo.GetShadingSurface(), toView, rng);

		CONSTS->rwRaysDirections[traceCommandIndex] = PackHalf2x16Norm(OctahedronEncodeNormal(sample.direction));
		CONSTS->rwRaysPdfs[traceCommandIndex]       = sample.pdf;
	}
}
