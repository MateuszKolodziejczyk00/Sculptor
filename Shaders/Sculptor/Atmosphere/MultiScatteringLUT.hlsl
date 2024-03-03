#include "SculptorShader.hlsli"

[[descriptor_set(RenderMultiScatteringLUTDS, 0)]]

#include "Atmosphere/Atmosphere.hlsli"
#include "Utils/Shapes.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


struct MultiScatteringResult
{
	float3 secondOrderInScatteringLuminance;
	float3 multiScatteringLuminanceFactor;
};


static const float multiScatteringSteps = 20.f;
static const int sqrtDirectionsNum = 8;


MultiScatteringResult ComputeMultiScatteringValues(in float3 location, in float3 sunDirection)
{
	MultiScatteringResult result;
	result.secondOrderInScatteringLuminance = 0.f;
	result.multiScatteringLuminanceFactor = 0.f;

	const float rcpSqrtDirections = rcp(float(sqrtDirectionsNum));
	const float rcpDirections = Pow2(rcpSqrtDirections);

	const float rcpMultiScatteringSteps = rcp(multiScatteringSteps);

	const Sphere groundSphere    = Sphere::Create(ZERO_VECTOR, u_atmosphereParams.groundRadiusMM);
	const Sphere atmophereSphere = Sphere::Create(ZERO_VECTOR, u_atmosphereParams.atmosphereRadiusMM);

	for (int i = 0; i < sqrtDirectionsNum; ++i)
	{
		for (int j = 0; j < sqrtDirectionsNum; ++j)
		{
			const float phi = PI * (i + 0.5f) * rcpSqrtDirections;
			float sinPhi, cosPhi;
			sincos(phi, sinPhi, cosPhi);

			const float cosTheta = 1.f - 2.f * (float(j) + 0.5f) * rcpSqrtDirections;
			const float sinTheta = sqrt(1.f - Pow2(cosTheta));

			const float3 rayDirection = float3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);

			const Ray ray = Ray::Create(location, rayDirection);

			const float atmopshereDist = ray.IntersectSphere(atmophereSphere).GetTime();
			const float groundDist     = ray.IntersectSphere(groundSphere).GetTime();
			const float rayDist = groundDist > 0.f ? groundDist : atmopshereDist;

			const float cosAlpha = dot(rayDirection, sunDirection);

			const float miePhaseValue = GetMiePhase(cosAlpha);
			const float rayleightPhaseValue = GetRayleighPhase(-cosAlpha);

			float3 luminance = 0.f;
			float3 scatteringLuminanceFactor = 0.f;
			float3 transmittance = 1.f;

			float currentT = 0.f;
			for (float k = 0.f; k < multiScatteringSteps; k += 1.f)
			{
				const float newT = (k + 0.3f) * rcpMultiScatteringSteps * rayDist;
				const float dt = newT - currentT;
				currentT = newT;

				const float3 sampleLocation = ray.origin + ray.direction * newT;

				const ScatteringValues scatteringValues = ComputeScatteringValues(u_atmosphereParams, sampleLocation);

				const float3 sampleTransmittance = exp(-dt * scatteringValues.extinction);

				const float3 scatteringNoPhase = scatteringValues.mieScattering + scatteringValues.rayleighScattering;
				const float3 scatteringFactor = (scatteringNoPhase / scatteringValues.extinction) * (1.f - sampleTransmittance);
				scatteringLuminanceFactor += transmittance * scatteringFactor;

				const float3 sunTransmittance = GetTransmittanceFromLUT(u_atmosphereParams, u_transmittanceLUT, u_linearSampler, sampleLocation, sunDirection);

				const float3 rayleighScattering = scatteringValues.rayleighScattering * rayleightPhaseValue;
				const float3 mieScattering = scatteringValues.mieScattering * miePhaseValue;
				const float3 inScattering = (rayleighScattering + mieScattering) * sunTransmittance;

				const float3 scatteringIntegral = (inScattering / scatteringValues.extinction) * (1.f - sampleTransmittance);

				luminance += scatteringIntegral * transmittance;
				transmittance *= sampleTransmittance;
			}

			if(groundDist > 0.f)
			{
				float3 hitLocation = ray.origin + ray.direction * groundDist;
				if(dot(hitLocation, sunDirection) > 0.f)
				{
					hitLocation = normalize(hitLocation) * u_atmosphereParams.groundRadiusMM;
					const float3 sunTransmittanceToGround = GetTransmittanceFromLUT(u_atmosphereParams, u_transmittanceLUT, u_linearSampler, hitLocation, sunDirection);
					luminance += transmittance * u_atmosphereParams.groundAlbedo * sunTransmittanceToGround;
				}
			}

			result.multiScatteringLuminanceFactor += scatteringLuminanceFactor * rcpDirections;
			result.secondOrderInScatteringLuminance += luminance * rcpDirections;
		}
	}
	
	return result;
}


[numthreads(8, 8, 1)]
void RenderMultiScatteringLUTCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;
	
	uint2 outputRes;
	u_multiScatteringLUT.GetDimensions(outputRes.x, outputRes.y);

	if(pixel.x < outputRes.x && pixel.y < outputRes.y)
	{
		const float2 uv = (pixel + 0.5f) / float2(outputRes);
		
		const float cosSunTheta = uv.x * 2.f - 1.f;
		const float sunTheta = acos(cosSunTheta);
		
		const float heightMM = lerp(u_atmosphereParams.groundRadiusMM, u_atmosphereParams.atmosphereRadiusMM, uv.y);

		const float3 location = float3(0.f, 0.f, heightMM);

		const float3 sunDir = float3(-sin(sunTheta), 0.f, cosSunTheta);

		const MultiScatteringResult multiScattering = ComputeMultiScatteringValues(location, sunDir);

		const float3 infiniteScatteringFactor = rcp(1.f - multiScattering.multiScatteringLuminanceFactor);
		const float3 psi = multiScattering.secondOrderInScatteringLuminance * infiniteScatteringFactor;

		u_multiScatteringLUT[pixel] = psi;
	}
}
