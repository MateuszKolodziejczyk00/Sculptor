#include "SculptorShader.hlsli"

[[shader_params(SkyInScatteringLUTConstants, CONSTS)]]

#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void RenderSkyInScatteringLUTCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;
	
	const float2 uv = (pixel + 0.5f) * CONSTS->rcpSkyInScatteringLUTRes;

	const float height = uv.x * HEIGHT_FOG_MAX_HEIGHT;

	const float cosTheta = uv.y * 2.f - 1.f;

	const float3 rayDirection = float3(sqrt(1.f - cosTheta * cosTheta), 0.f, cosTheta);

	const float3 rayOrigin = float3(0.f, 0.f, height);

	const uint stepsNum = 32u;
	const float tMax = 2000.f;

	const float stepSize = tMax / stepsNum;

	const float3 fogAlbedo = CONSTS->heightFogParams.albedo;

	float3 integratedScattering    = 0.f;
	float integratedTransmittance = 1.f;

	float currentT = 0.f;

	for (float t = 1.f; t < tMax; t += stepSize)
	{
		const float3 currentLocation = rayOrigin + rayDirection * t;
		const float density = EvaluateHeightFogDensityAtLocation(CONSTS->heightFogParams, currentLocation);

		const float4 fogScatteringExtinction = ComputeScatteringAndExtinction(fogAlbedo, CONSTS->heightFogParams.extinction, density);

		float inScatteredIlluminance = 1.f;
		inScatteredIlluminance *= EvaluateHeightBasedTransmittanceForSegment(CONSTS->heightFogParams, currentLocation, currentLocation + float3(0.f, 0.f, 1.f) * 2000.f);

		const float phaseFunction = 1.f / (4.f * PI); // For now assume isotropic scattering

		const float3 inScattering = inScatteredIlluminance * phaseFunction * fogScatteringExtinction.rgb;
		const float extinction = fogScatteringExtinction.w;

		const float clampedExtinction = max(extinction, 0.0001f);

		const float transmittance = exp(-clampedExtinction * (t - currentT));

		const float3 scattering = (inScattering * (1.f - transmittance)) / clampedExtinction;

		integratedScattering += scattering * integratedTransmittance;
		integratedTransmittance *= max(transmittance, 0.f);

		currentT = t;
	}

	CONSTS->rwSkyInScatteringLUT[pixel] = float4(integratedScattering, 0.f);
}
