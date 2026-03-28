#include "SculptorShader.hlsli"

[[shader_params(GenerateOcclusionMapConstants, u_constants)]]

#include "Utils/Random.hlsli"


#define POM_PIXEL_FOOTPRINT 0.01f

#define QUALITY 0 // 0 = GTAO, 1 = Stochastic AO


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


float FindHorizonCos(in float2 startCoords, in float2 dir, in float centerHeight, in uint2 resolution, in float2 rcpResolution)
{
	float h0Cos = 0.f;

	for (float t = 1.f; t < 64.f; t += 1.f)
	{
		const float2 sampleCoords = startCoords + dir * t;

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
				continue;
			}
		}

		const float sampleHeight = 1.f - u_constants.depth.SampleLevel(BindlessSamplers::LinearClampEdge(), (float2(sampleCoords) + 0.5f) * rcpResolution, 0);

		const float3 delta = float3((sampleCoords - startCoords) * POM_PIXEL_FOOTPRINT, (sampleHeight - centerHeight) * u_constants.maxDepthCm);

		const float horizonCos = normalize(delta).z;
		h0Cos = max(h0Cos, horizonCos);
	}

	return h0Cos;
}


float EvaluateGTAO(in int2 coords, in uint2 resolution, in float2 rcpResolution, in float3 normal)
{
	const float slicesNum = 32.f;
	const float angleStep = PI / slicesNum;

	const float centerHeight = 1.f - u_constants.depth.Load(coords);

	const float rayLength = 10.f;

	const float n = acos(normal.z);

	float occlusionSum = 0.f;

	for (float i = 0; i < slicesNum; ++i)
	{
		const float angle = i * angleStep;
		const float2 dir = float2(cos(angle), sin(angle));

		const float h0Cos = FindHorizonCos(coords, dir, centerHeight, resolution, rcpResolution);
		const float h1Cos = FindHorizonCos(coords, -dir, centerHeight, resolution, rcpResolution);

		const float h0 = acos(h0Cos);
		const float h1 = acos(h1Cos);

		const float occlusion = 0.25f * (-cos(2.f * h1 - n) + cos(n) + 2.f * h1 * sin(n) + -cos(2.f * h0 - n) + cos(n) + 2.f * h0 * sin(n));

		occlusionSum += occlusion;
	}

	return (occlusionSum / slicesNum);
}


float EvaluateStochasticAO(in int2 coords, in uint2 resolution, in float2 rcpResolution, in float3 normal)
{
	const float2 random = float2(Random(float2(coords) + 2137.f), Random(float2(coords.yx) + 3212.f));

	const float3 tangent = abs(dot(normal, UP_VECTOR)) > 0.9f ? cross(normal, RIGHT_VECTOR) : cross(normal, UP_VECTOR);
	const float3 bitangent = cross(normal, tangent);
	const float3x3 tangentSpace = transpose(float3x3(tangent, bitangent, normal));

	RngState rng = RngState::Create(coords, 2137u);

	const float centerHeight = 1.f - u_constants.depth.Load(coords) * u_constants.maxDepthCm;
	const float3 origin = float3((coords + 0.5f) * POM_PIXEL_FOOTPRINT, centerHeight);

	float visSum = 0.f;
	for (uint rayIdx = 0u; rayIdx < 256u; ++rayIdx)
	{
		const float2 random = float2(rng.Next(), rng.Next());
		float pdf = 0.f;
		const float3 rayDirection = RandomVectorInCosineWeightedHemisphere(tangentSpace, random, OUT pdf);

		float rayVisibility = 1.f;
		const float rayLenght = POM_PIXEL_FOOTPRINT * 30.f;
		for (float t = POM_PIXEL_FOOTPRINT; t < rayLenght; t += POM_PIXEL_FOOTPRINT)
		{
			const float3 tracePos = origin + t * rayDirection;
			if (tracePos.z >= u_constants.maxDepthCm)
			{
				break;
			}

			const float2 uv = tracePos.xy / POM_PIXEL_FOOTPRINT * rcpResolution;
			if (any(saturate(uv) != uv))
			{
				break;
			}

			const float sampleHeight = 1.f - u_constants.depth.SampleLevel(BindlessSamplers::LinearClampEdge(), uv, 0) * u_constants.maxDepthCm;

			const float bias = 0.01f;
			if (tracePos.z + bias < sampleHeight)
			{
				rayVisibility = 0.f;
				break;
			}
		}

		visSum += rayVisibility;
	}

	const float occlusion = (visSum / 256.f);
	return occlusion;
}


[numthreads(16, 16, 1)]
void GenerateCS(CS_INPUT input)
{
	const int2 coords = input.globalID.xy;

	const uint2 resolution = u_constants.rwOcclusion.GetResolution();
	if (any(coords >= resolution))
	{
		return;
	}

	const float2 rcpResolution = 1.f / resolution;

	float3 normal = u_constants.normals.Load(coords).xyz;
	normal.xy = normal.xy * 2.f - 1.f;
	normal = normalize(normal);

#if QUALITY == 0
	const float occlusion = EvaluateGTAO(coords, resolution, rcpResolution, normal);
#elif QUALITY == 1
	const float occlusion = EvaluateStochasticAO(coords, resolution, rcpResolution, normal);
#endif //QUALITY

	u_constants.rwOcclusion.Store(coords, occlusion);
}
