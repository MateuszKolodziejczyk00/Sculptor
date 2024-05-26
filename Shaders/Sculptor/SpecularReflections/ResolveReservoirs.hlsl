#include "SculptorShader.hlsli"

[[descriptor_set(ResolveReservoirsDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "SpecularReflections/SRReservoir.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Shading/Shading.hlsli"
#include "SpecularReflections/SpecularReflectionsCommon.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void ResolveReservoirsCS(CS_INPUT input)
{
	const int2 pixel = input.globalID.xy;
	
	if(all(pixel < u_resolveConstants.resolution))
	{
		const uint reservoirIdx = GetScreenReservoirIdx(pixel, u_resolveConstants.resolution);
		const SRReservoir reservoir = UnpackReservoir(u_reservoirsBuffer[reservoirIdx]);

		const float depth = u_depthTexture.Load(uint3(pixel, 0));
		const float2 uv = (pixel + 0.5f) * u_resolveConstants.pixelSize;
		const float3 ndc = float3(uv * 2.f - 1.f, depth);

		const float3 sampleLocation = NDCToWorldSpace(ndc, u_sceneView);

		const float3 sampleNormal = u_normalsTexture.Load(uint3(pixel, 0)).xyz * 2.f - 1.f;

		const float3 toView = normalize(u_sceneView.viewLocation - sampleLocation);

		const float roughness = u_roughnessTexture.Load(uint3(pixel, 0));
		const float3 f0 = u_specularColorTexture.Load(uint3(pixel, 0)).rgb;

		const float hitDistance = length(reservoir.hitLocation - sampleLocation);
		const float3 lightDir = (reservoir.hitLocation - sampleLocation) / hitDistance;

		float3 luminance = reservoir.luminance * reservoir.weightSum;

		float3 Lo = 0.f;
		if(reservoir.HasFlag(SR_RESERVOIR_FLAGS_DDGI_TRACE))
		{
			// use preintegrated brdf for ddgi traces
			const float NdotV = saturate(dot(sampleNormal, toView));
			const float2 integratedBRDF = u_brdfIntegrationLUT.SampleLevel(u_brdfIntegrationLUTSampler, float2(NdotV, roughness), 0);

			Lo = luminance * (f0 * integratedBRDF.x + integratedBRDF.y);
		}
		else
		{
			const float3 specular = SR_GGX_Specular(sampleNormal, toView, lightDir, roughness, f0);
			Lo = specular * luminance; 
		}

		// demodulate specular
		Lo /= max(0.01f, f0);

		u_luminanceHitDistanceTexture[pixel] = float4(Lo, hitDistance);
	}
}
