#include "SculptorShader.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "Utils/Shapes.hlsli"

[[descriptor_set(RenderTransmittanceLUTDS, 0)]]


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


float3 ComputeSunTransmittance(float3 location, float3 sunDirection, float stepsNum)
{
	const Ray rayToSun        = Ray::Create(location, sunDirection);
	const Sphere groundSphere = Sphere::Create(ZERO_VECTOR, u_atmosphereParams.groundRadiusMM);

	if(rayToSun.IntersectSphere(groundSphere).IsValid())
	{
		return 0.f;
	}

	const Sphere atmosphereSphere = Sphere::Create(ZERO_VECTOR, u_atmosphereParams.atmosphereRadiusMM);

	const IntersectionResult atmosphereIntersection = rayToSun.IntersectSphere(atmosphereSphere);

	const float rcpStepsNum = rcp(stepsNum);

	float t = 0.f;
	float3 transmittance = 1.f;

	for (float i = 0; i < stepsNum; i += 1.f)
	{
		const float newT = (i + 0.3f) * rcpStepsNum * atmosphereIntersection.GetTime();
		const float dt = newT - t;
		t = newT;

		const float3 currentLocation = rayToSun.origin + rayToSun.direction * t;

		const ScatteringValues scatteringValues = ComputeScatteringValues(u_atmosphereParams, currentLocation);

		transmittance *= exp(-dt * scatteringValues.extinction);
	}
	
	return transmittance;
}


[numthreads(8, 8, 1)]
void RenderTransmittanceLUTCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;
	
	uint2 outputRes;
	u_transmittanceLUT.GetDimensions(outputRes.x, outputRes.y);

	if(pixel.x < outputRes.x && pixel.y < outputRes.y)
	{
		const float2 uv = (pixel + 0.5f) / float2(outputRes);
		
		const float cosSunTheta = uv.x * 2.f - 1.f;
		const float sinSunTheta = sqrt(1.f - Pow2(cosSunTheta));
		
		const float heightMM = lerp(u_atmosphereParams.groundRadiusMM, u_atmosphereParams.atmosphereRadiusMM, uv.y);

		const float3 location = float3(0.f, 0.f, heightMM);

		const float3 sunDir = float3(-sinSunTheta, 0.f, cosSunTheta);

		const uint stepsNum = 40;

		u_transmittanceLUT[pixel] = ComputeSunTransmittance(location, sunDir, stepsNum);
	}
}
