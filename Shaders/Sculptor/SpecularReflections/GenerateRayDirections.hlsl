#include "SculptorShader.hlsli"

[[descriptor_set(GenerateRayDirectionsDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"
#include "Utils/Random.hlsli"

#include "SpecularReflections\SpecularReflectionsCommon.hlsli"

#include "Shading/Shading.hlsli"

#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"
#include "Utils/VariableRate/VariableRate.hlsli"


struct RayDirectionInfo
{
	float3 direction;
	float  pdf;
};


RayDirectionInfo GenerateReflectionRayDir(in float3 normal, in float roughness, in float3 toView, in uint2 pixel)
{
	RayDirectionInfo result;

	if(roughness < SPECULAR_TRACE_MAX_ROUGHNESS)
	{
		result.direction = reflect(-toView, normal);
		result.pdf       = -1.f; // -1 means specular trace
	}
	else
	{
		RngState rng = RngState::Create(pixel, u_constants.seed);

		uint attempt = 0u;

		while(true)
		{
			const float2 noise = float2(rng.Next(), rng.Next());

			const float3 h = SampleVNDFIsotropic(noise, toView, roughness, normal);

			const float3 reflectedRay = reflect(-toView, h);

			if(dot(reflectedRay, normal) > 0.f)
			{
				result.pdf       = PDFVNDFIsotrpic(toView, reflectedRay, roughness, normal);
				result.direction = reflectedRay;

				break;
			}
			else if (attempt == 16u)
			{
				result.direction = 0.f;
				result.pdf       = 0.f;

				break;
			}

			++attempt;
		}
	}

	return result;
}


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(32, 1, 1)]
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

		const float3 normal   = OctahedronDecodeNormal(u_normalsTexture.Load(uint3(pixel, 0)));
		const float roughness = u_roughnessTexture.Load(uint3(pixel, 0));

		const float3 toView = normalize(u_sceneView.viewLocation - worldLocation);

		const RayDirectionInfo rayDirection = GenerateReflectionRayDir(normal, roughness, toView, pixel);

		u_rwRaysDirections[traceCommandIndex] = PackHalf2x16Norm(OctahedronEncodeNormal(rayDirection.direction));
		u_rwRaysPdfs[traceCommandIndex]       = rayDirection.pdf;
	}
}
