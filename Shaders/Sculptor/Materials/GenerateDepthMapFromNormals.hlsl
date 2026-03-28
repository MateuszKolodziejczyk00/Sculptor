#include "SculptorShader.hlsli"

[[shader_params(GenerateDepthMapFromNormalsConstants, u_constants)]]

#define POM_PIXEL_FOOTPRINT 0.01f


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(16, 16, 1)]
void GenerateCS(CS_INPUT input)
{
	const int2 coords = input.globalID.xy;

	const uint2 resolution = u_constants.rwDepth.GetResolution();
	if (any(coords >= resolution))
	{
		return;
	}

	const float2 rcpResolution = 1.f / resolution;

	const float raysNum = 64.f;
	const float angleStep = 2.f * PI / raysNum;

	const float rayLength = 64.f;

	float heightSum = 0.f;
	float samplesNum = 0.f;

	for (float i = 0; i < raysNum; ++i)
	{
		const float angle = i * angleStep;
		const float2 dir = float2(cos(angle), sin(angle));

		float rayHeight = 0.f;

		for (float t = 1.f; t < rayLength; t += 1.f)
		{
			const float2 sampleCoords = coords + dir * t;

			if (any(sampleCoords < 0) || any(sampleCoords >= resolution))
			{
				break;
			}

			if (u_constants.alpha.IsValid())
			{
				const float2 sampleUV = (float2(sampleCoords) + 0.5f) * rcpResolution;
				const float alpha = u_constants.alpha.SampleLevel(BindlessSamplers::LinearClampEdge(), sampleUV, 0);
				if (alpha < 0.5f)
				{
					break;
				}
			}

			float3 normal = u_constants.normals.Load(int2(sampleCoords)).xyz;
			normal.xy = normal.xy * 2.f - 1.f;
			normal = normalize(normal);

			const float slope = -dot(normal, float3(dir, 0.f));
			const float heightDiff = slope * POM_PIXEL_FOOTPRINT;

			rayHeight += heightDiff;

			samplesNum += 1.f;
			heightSum += rayHeight;
		}
	}

	const float strength = 0.2f;

	float finalHeight = -(heightSum / samplesNum);
	finalHeight /= u_constants.maxDepthCm;
	finalHeight *= strength;
	finalHeight = saturate(finalHeight + 1.f);

	u_constants.rwDepth.Store(coords, 1.f - finalHeight);
}
