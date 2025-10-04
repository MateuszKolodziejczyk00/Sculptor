#include "SculptorShader.hlsli"

[[descriptor_set(ResolveReservoirsDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "SpecularReflections/SRReservoir.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"
#include "Shading/Shading.hlsli"
#include "SpecularReflections/RTGICommon.hlsli"
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

		const bool canUseReservoir = reservoir.IsValid() && reservoir.HasValidResult();

		if(!canUseReservoir && !isTracingCoord)
		{
			reservoirCoords = traceCoords;
			const uint tracingReservoirIdx = GetScreenReservoirIdx(traceCoords, u_resamplingConstants.reservoirsResolution);
			reservoir = UnpackReservoir(u_reservoirsBuffer[tracingReservoirIdx]);
		}

		float3 specularLo = 0.f;
		float3 diffuseLo  = 0.f;
		float hitDistance = SPT_NAN;

		if(reservoir.IsValid() && reservoir.HasValidResult())
		{
			const float depth = u_depthTexture.Load(uint3(reservoirCoords, 0));
			const float2 uv = (reservoirCoords + 0.5f) * u_resamplingConstants.pixelSize;
			const float3 ndc = float3(uv * 2.f - 1.f, depth);
	
			const float3 sampleLocation = NDCToWorldSpace(ndc, u_sceneView);
	
			const float3 sampleNormal = OctahedronDecodeNormal(u_normalsTexture.Load(uint3(reservoirCoords, 0)));
	
			const float3 toView = normalize(u_sceneView.viewLocation - sampleLocation);
	
			const float roughness = u_roughnessTexture.Load(uint3(reservoirCoords, 0));

			const float4 baseColorMetallic = u_baseColorTexture.Load(uint3(reservoirCoords, 0));

			float f0;
			float3 diffuseColor;
			ComputeSurfaceColor(baseColorMetallic.rgb, baseColorMetallic.w, OUT diffuseColor, OUT f0);
	
			hitDistance = length(reservoir.hitLocation - sampleLocation);
			const float3 lightDir = (reservoir.hitLocation - sampleLocation) / hitDistance;
	
			const float3 luminance = reservoir.luminance * reservoir.weightSum;
	
			const RTBRDF brdf = RT_EvaluateBRDF(sampleNormal, toView, lightDir, roughness, f0, diffuseColor);
			specularLo = brdf.specular * luminance * dot(sampleNormal, lightDir);
	
			const float NdotV = saturate(dot(sampleNormal, toView));
			const float2 integratedBRDF = u_brdfIntegrationLUT.SampleLevel(u_brdfIntegrationLUTSampler, float2(NdotV, roughness), 0);
	
			if(any(isnan(specularLo)) || any(isinf(specularLo)))
			{
				specularLo = 0.f;
			}
	
			// demodulate specular
			specularLo /= max((f0 * integratedBRDF.x + integratedBRDF.y), 0.01f);

			const float NdotL = saturate(dot(sampleNormal, lightDir));
			diffuseLo = luminance * NdotL; // demodulated without lambertian term for denoise
		}

		u_specularLumHitDistanceTexture[pixel] = float4(specularLo, hitDistance);
		u_diffuseLumHitDistanceTexture[pixel]  = float4(diffuseLo, hitDistance);
	}
}