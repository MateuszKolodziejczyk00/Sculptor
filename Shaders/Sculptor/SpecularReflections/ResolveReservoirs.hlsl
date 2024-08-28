#include "SculptorShader.hlsli"

[[descriptor_set(ResolveReservoirsDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "SpecularReflections/SRReservoir.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"
#include "Shading/Shading.hlsli"
#include "SpecularReflections/SpecularReflectionsCommon.hlsli"
#include "Utils/VariableRate/VariableRate.hlsli"

struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void ResolveReservoirsCS(CS_INPUT input)
{
	uint2 pixel = input.globalID.xy;
	
	if(all(pixel < u_resamplingConstants.resolution))
	{
		uint2 reservoirCoords = pixel;

		const uint reservoirIdx = GetScreenReservoirIdx(pixel, u_resamplingConstants.reservoirsResolution);
		SRReservoir reservoir = UnpackReservoir(u_reservoirsBuffer[reservoirIdx]);

		const uint2 traceCoords = GetVariableTraceCoords(u_variableRateBlocksTexture, pixel);

		const bool isTracingCoord = all(pixel == traceCoords);

		if(!reservoir.IsValid() && !isTracingCoord)
		{
			reservoirCoords = traceCoords;
			const uint tracingReservoirIdx = GetScreenReservoirIdx(traceCoords, u_resamplingConstants.reservoirsResolution);
			reservoir = UnpackReservoir(u_reservoirsBuffer[tracingReservoirIdx]);
		}

		const float depth = u_depthTexture.Load(uint3(reservoirCoords, 0));
		const float2 uv = (reservoirCoords + 0.5f) * u_resamplingConstants.pixelSize;
		const float3 ndc = float3(uv * 2.f - 1.f, depth);

		const float3 sampleLocation = NDCToWorldSpace(ndc, u_sceneView);

		const float3 sampleNormal = OctahedronDecodeNormal(u_normalsTexture.Load(uint3(reservoirCoords, 0)));

		const float3 toView = normalize(u_sceneView.viewLocation - sampleLocation);

		const float roughness = u_roughnessTexture.Load(uint3(reservoirCoords, 0));
		const float3 f0 = u_specularColorTexture.Load(uint3(reservoirCoords, 0)).rgb;

		const float hitDistance = length(reservoir.hitLocation - sampleLocation);
		const float3 lightDir = (reservoir.hitLocation - sampleLocation) / hitDistance;

		float3 luminance = reservoir.luminance * reservoir.weightSum;

		const float3 specular = SR_GGX_Specular(sampleNormal, toView, lightDir, roughness, f0);
		float3 Lo = specular * luminance * dot(sampleNormal, lightDir);

		const float NdotV = saturate(dot(sampleNormal, toView));
		const float2 integratedBRDF = u_brdfIntegrationLUT.SampleLevel(u_brdfIntegrationLUTSampler, float2(NdotV, roughness), 0);

		if(any(isnan(Lo)) || any(isinf(Lo)))
		{
			Lo = 0.f;
		}

		// demodulate specular
		Lo /= max((f0 * integratedBRDF.x + integratedBRDF.y), 0.01f);

		u_luminanceHitDistanceTexture[pixel] = float4(Lo, hitDistance);
	}
}