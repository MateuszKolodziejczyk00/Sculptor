#include "Lights/LightsTiles.hlsli"
#include "Lights/LightingUtils.hlsli"
#include "Lights/Shadows.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"
#include "RayTracing/RTScene.hlsli"
#include "SceneRendering/WSC.hlsli"

#ifdef PARAM_DDGIGPUScene
#include "DDGI/DDGITypes.hlsli"
#endif // PARAM_DDGIGPUScene

#ifdef PARAM_ViewShadingParams

// Based on https://themaister.net/blog/2020/01/10/clustered-shading-evolution-in-granite/
uint ClusterMaskRange(uint mask, uint2 range, uint startIdx)
{
	range.x = clamp(range.x, startIdx, startIdx + 32u);
	range.y = clamp(range.y + 1u, range.x, startIdx + 32u);

	const uint numBits = range.y - range.x;
	const uint rangeMask = numBits == 32 ? 0xffffffffu : ((1u << numBits) - 1u) << (range.x - startIdx);
	return mask & uint(rangeMask);
}

void CalcReflectedLuminance<TLightingAccumulator : ILightingAccumulator>(in ShadedSurface surface, in float3 viewDir, inout TLightingAccumulator accumulator)
{
	// Directional Lights

	{
		for (uint i = 0; i < PARAM_ViewShadingParams->lightsData->directionalLightsNum; ++i)
		{
			const DirectionalLightGPUData directionalLight = PARAM_ViewShadingParams->directionalLights[i];

			const float3 illuminance = directionalLight.illuminance;

			if (any(illuminance > 0.f) && dot(-directionalLight.direction, surface.shadingNormal) > 0.f)
			{
				float visibility = PARAM_ViewShadingParams->shadowMask.SampleLevel(BindlessSamplers::NearestClampEdge(), surface.uv, 0).x;

				if(visibility > 0.f)
				{
					const float fogTransmittance = EvaluateHeightBasedTransmittanceForSegment(PARAM_ViewShadingParams->lightsData->heightFog, surface.location, surface.location - directionalLight.direction * 1500.f);

					const float attenuation = fogTransmittance * visibility;
					const LightingContribution shadingRes = CalcLighting(surface, -directionalLight.direction, viewDir, illuminance) * attenuation;
					accumulator.Accumulate(shadingRes);
				}
			}
		}
	}

	// Point lights
	const uint2 lightsTileCoords = GetLightsTile(surface.uv, PARAM_ViewShadingParams->lightsData->tileSize);
	const uint tileLightsDataOffset = GetLightsTileDataOffset(lightsTileCoords, PARAM_ViewShadingParams->lightsData->tilesNum, PARAM_ViewShadingParams->lightsData->localLights32Num);
	
	const uint clusterIdx = surface.linearDepth / PARAM_ViewShadingParams->lightsData->zClusterLength;
	const uint2 clusterRange = clusterIdx < PARAM_ViewShadingParams->lightsData->zClustersNum ? PARAM_ViewShadingParams->clustersRanges[clusterIdx] : uint2(0u, 0u);
	
	{
		for(uint i = 0; i < PARAM_ViewShadingParams->lightsData->localLights32Num; ++i)
		{
			uint lightsMask = PARAM_ViewShadingParams->tilesLightsMask[tileLightsDataOffset + i];
			lightsMask = ClusterMaskRange(lightsMask, clusterRange, i << 5u);

			while(lightsMask)
			{
				const uint maskBitIdx = firstbitlow(lightsMask);

				const uint lightIdx = i * 32 + maskBitIdx;
				const LocalLightInterface localLight = PARAM_ViewShadingParams->localLights[lightIdx];
			  
				const float3 toLight = localLight.location - surface.location;

				if (dot(toLight, surface.shadingNormal) > 0.f)
				{
					const float distToLight = length(toLight);

					if (distToLight < localLight.range)
					{
						const float3 lightDir = toLight / distToLight;
						const float3 illuminance = GetLightIlluminanceAtLocation(localLight, surface.location);

						if(any(illuminance > 0.f))
						{
							float visibility = 1.f;
							if (localLight.shadowMapFirstFaceIdx != IDX_NONE_32)
							{
								visibility = EvaluatePointLightShadows(surface, localLight.location, localLight.range, localLight.shadowMapFirstFaceIdx);
							}
					
							if (visibility > 0.f)
							{
								const LightingContribution shadingRes = CalcLighting(surface, lightDir, viewDir, illuminance) * visibility;
								accumulator.Accumulate(shadingRes);
							}
						}
					}
				}

				lightsMask &= ~(1u << maskBitIdx);
			}
		}
	}
}


#if SPT_META_PARAM_DEBUG_FEATURES
void TiledShadingDebug(in uint2 pixelCoords, in ShadedSurface surface)
{
	const uint2 lightsTileCoords = GetLightsTile(surface.uv, PARAM_ViewShadingParams->lightsData->tileSize);
	const uint tileLightsDataOffset = GetLightsTileDataOffset(lightsTileCoords, PARAM_ViewShadingParams->lightsData->tilesNum, PARAM_ViewShadingParams->lightsData->localLights32Num);
	
	const uint clusterIdx = surface.linearDepth / PARAM_ViewShadingParams->lightsData->zClusterLength;
	const uint2 clusterRange = clusterIdx < PARAM_ViewShadingParams->lightsData->zClustersNum ? PARAM_ViewShadingParams->clustersRanges[clusterIdx] : uint2(0u, 0u);

	uint lightsNum = 0u;

	for(uint i = 0; i < PARAM_ViewShadingParams->lightsData->localLights32Num; ++i)
	{
		uint lightsMask = PARAM_ViewShadingParams->tilesLightsMask[tileLightsDataOffset + i];
		lightsMask = ClusterMaskRange(lightsMask, clusterRange, i << 5u);

		while(lightsMask)
		{
			const uint maskBitIdx = firstbitlow(lightsMask);

			const uint lightIdx = i * 32 + maskBitIdx;
			const LocalLightInterface localLight = PARAM_ViewShadingParams->localLights[lightIdx];

			++lightsNum;

			lightsMask &= ~(1u << maskBitIdx);
		}
	}

	if(lightsNum > 0u)
	{
		debug::WriteDebugPixelOnScreen(pixelCoords, float4(lerp(float3(0.f, 1.f, 1.f), float3(1.f, 0.f, 0.f), saturate(lightsNum / 10u)), 0.5f));
	}
}
#endif // SPT_META_PARAM_DEBUG_FEATURES


struct InScatteringParams
{
	float2 uv;
	float linearDepth;
	
	float3 worldLocation;

	float3 toViewNormal;

	float phaseFunctionAnisotrophy;

	float3 inScatteringColor;

	float froxelDepthRange;

	float directionalLightShadowTerm;
};


float3 ComputeDirectionalLightsInScattering(in InScatteringParams params)
{
	float3 inScattering = 0.f;

	for (uint i = 0; i < PARAM_ViewShadingParams->lightsData->directionalLightsNum; ++i)
	{
		const DirectionalLightGPUData directionalLight = PARAM_ViewShadingParams->directionalLights[i];

		const float3 illuminance = directionalLight.illuminance;

		if(params.directionalLightShadowTerm > 0.f)
		{
			inScattering += params.directionalLightShadowTerm * illuminance * PhaseFunction(params.toViewNormal, directionalLight.direction, params.phaseFunctionAnisotrophy);
		}
	}

	return inScattering * params.inScatteringColor;
}


float3 ComputeLocalLightsInScattering(in InScatteringParams params)
{
	float3 inScattering = 0.f;
	
	const uint2 lightsTileCoords = GetLightsTile(params.uv, PARAM_ViewShadingParams->lightsData->tileSize);
	const uint tileLightsDataOffset = GetLightsTileDataOffset(lightsTileCoords, PARAM_ViewShadingParams->lightsData->tilesNum, PARAM_ViewShadingParams->lightsData->localLights32Num);
	
	const uint clusterIdx = params.linearDepth / PARAM_ViewShadingParams->lightsData->zClusterLength;
	const uint2 clusterRange = clusterIdx < PARAM_ViewShadingParams->lightsData->zClustersNum ? PARAM_ViewShadingParams->clustersRanges[clusterIdx] : uint2(0u, 0u);
	
	for(uint i = 0; i < PARAM_ViewShadingParams->lightsData->localLights32Num; ++i)
	{
		uint lightsMask = PARAM_ViewShadingParams->tilesLightsMask[tileLightsDataOffset + i];
		lightsMask = ClusterMaskRange(lightsMask, clusterRange, i << 5u);

		while(lightsMask)
		{
			const uint maskBitIdx = firstbitlow(lightsMask);

			const uint lightIdx = i * 32 + maskBitIdx;
			const LocalLightInterface localLight = PARAM_ViewShadingParams->localLights[lightIdx];
		  
			const float3 toLight = localLight.location - params.worldLocation;

			const float distToLight = length(toLight);

			if(distToLight < localLight.range)
			{
				const float3 lightDir = toLight / distToLight;
				const float3 illuminance = GetLightIlluminanceAtLocation(localLight, params.worldLocation);

				if (any(illuminance > 0.f))
				{
					float visibility = 1.f;
					if (localLight.shadowMapFirstFaceIdx != IDX_NONE_32)
					{
						visibility = EvaluatePointLightShadowsAtLocation(params.worldLocation, localLight.location, localLight.range, localLight.shadowMapFirstFaceIdx);
					}
			
					if (visibility > 0.f)
					{
						inScattering += illuminance * visibility * PhaseFunction(params.toViewNormal, -lightDir, params.phaseFunctionAnisotrophy);
					}
				}
			}

			lightsMask &= ~(1u << maskBitIdx);
		}
	}

	inScattering *= params.inScatteringColor;
	
	return inScattering;
}

#endif // PARAM_ViewShadingParams

#ifdef PARAM_GlobalLightsParams

struct ShadowRayPayload
{
	bool isShadowed;
};


float3 CalcReflectedLuminance_Direct(in ShadedSurface surface, in float3 viewDir)
{
	float3 luminance = 0.f;

	// Directional Lights

	{
		for (uint i = 0; i < PARAM_GlobalLightsParams->directionalLightsNum; ++i)
		{
			const DirectionalLightGPUData directionalLight = PARAM_GlobalLightsParams->directionalLights[i];

			float3 lightIlluminance = directionalLight.illuminance;

			float cloudsTransmittance = 1.f;
			if(i == 0u && PARAM_GlobalLightsParams->hasValidCloudsTransmittanceMap)
			{
				const float4 ctmCS = mul(PARAM_GlobalLightsParams->cloudsTransmittanceViewProj, float4(surface.location, 1.f));
				if(all(ctmCS.xy <= ctmCS.w) && all(ctmCS.xy >= -ctmCS.w))
				{
					const float2 ctmUV = (ctmCS.xy / ctmCS.w) *	0.5f + 0.5f;
					cloudsTransmittance = PARAM_GlobalLightsParams->cloudsTransmittanceMap.SampleLevel(BindlessSamplers::LinearClampEdge(), ctmUV, 0.f);
				}
			}

			lightIlluminance *= cloudsTransmittance;

			const float fogTransmittance = EvaluateHeightBasedTransmittanceForSegment(PARAM_GlobalLightsParams->heightFog, surface.location, surface.location - directionalLight.direction * 1500.f);
			lightIlluminance *= fogTransmittance;

			if (any(lightIlluminance > 0.f) && dot(-directionalLight.direction, surface.shadingNormal) > 0.f)
			{
				RayDesc rayDesc;
				rayDesc.TMin        = 0.00f;
				rayDesc.TMax        = 50.f;
				rayDesc.Origin      = surface.location + surface.geometryNormal * 0.03f;
				rayDesc.Direction   = -directionalLight.direction;

				const float visibility = WSC().SampleShadows(surface.location, surface.geometryNormal);
				if (visibility > 0.f)
				{
					luminance += CalcLighting(surface, -directionalLight.direction, viewDir, lightIlluminance).sceneLuminance * visibility;
				}
			}
		}
	}

	// Local Lights

	{
		for(uint lightIdx = 0; lightIdx < PARAM_GlobalLightsParams->localLightsNum; ++lightIdx)
		{
			const LocalLightInterface localLight = PARAM_GlobalLightsParams->localLights[lightIdx];
			
			const float3 toLight = localLight.location - surface.location;

			if (dot(toLight, surface.shadingNormal) > 0.f)
			{
				const float distToLight = length(toLight);

				if (distToLight < localLight.range)
				{
					const float3 lightDir = toLight / distToLight;
					const float3 illuminance = GetLightIlluminanceAtLocation(localLight, surface.location);

					if(any(illuminance > 0.f))
					{
						float visibility = 1.f;
						if (localLight.shadowMapFirstFaceIdx != IDX_NONE_32)
						{
							const float3 biasedLocation  = surface.location + surface.shadingNormal * 0.02f;
							visibility = EvaluatePointLightShadowsAtLocation(surface.location, biasedLocation, localLight.range, localLight.shadowMapFirstFaceIdx);
						}
				
						if (visibility > 0.f)
						{
							luminance += CalcLighting(surface, lightDir, viewDir, illuminance).sceneLuminance * visibility;
						}
					}
				}
			}
		}
	}

	return luminance;
}

#ifdef PARAM_DDGIGPUScene
float3 CalcReflectedLuminance_Indirect<TDDGISampleContext : IDDGISampleContext>(in ShadedSurface surface, in float3 viewDir, in TDDGISampleContext ddgiSampleContext, in float indirectMultiplier)
#else
float3 CalcReflectedLuminance_Indirect(in ShadedSurface surface, in float3 viewDir)
#endif // PARAM_DDGIGPUScene
{
	float3 luminance = 0.f;

#if defined(PARAM_DDGIGPUScene)
	const float3 specularDominantDirection = GetSpecularDominantDirection(surface.geometryNormal, reflect(-viewDir, surface.geometryNormal), surface.roughness);
	const float specularWeight = Luminance(surface.specularColor) / Luminance(surface.specularColor + surface.diffuseColor);
	const float3 sampleDirection = normalize(lerp(surface.shadingNormal, specularDominantDirection, specularWeight));

	DDGISampleParams diffuseSampleParams = CreateDDGISampleParams(surface.location, surface.geometryNormal, viewDir);
	diffuseSampleParams.sampleDirection = surface.shadingNormal;
	diffuseSampleParams.sampleLocationBiasMultiplier = 1.0f;

	const float3 indirectLuminance = DDGISampleLuminance(diffuseSampleParams, ddgiSampleContext);
	const float3 indirectIlluminance = indirectLuminance * 2.f * PI;
	luminance += Diffuse_Lambert(indirectIlluminance) * surface.diffuseColor * indirectMultiplier;

	const float NdotL = dot(surface.shadingNormal, specularDominantDirection);
	const float NdotV = saturate(dot(surface.shadingNormal, viewDir));
	const float2 integratedBRDF = PARAM_GlobalLightsParams->brdfIntegrationLUT.SampleLevel(BindlessSamplers::LinearClampEdge(), float2(NdotV, surface.roughness), 0);
	luminance += indirectLuminance * (surface.specularColor * integratedBRDF.x + integratedBRDF.y) * indirectMultiplier * NdotL;
#endif // defined(PARAM_DDGIGPUScene)

	return luminance;
}

#ifdef PARAM_DDGIGPUScene
float3 CalcReflectedLuminance<TDDGISampleContext : IDDGISampleContext>(in ShadedSurface surface, in float3 viewDir, in TDDGISampleContext ddgiSampleContext, in float indirectMultiplier)
#else
float3 CalcReflectedLuminance(in ShadedSurface surface, in float3 viewDir)
#endif // PARAM_DDGIGPUScene
{
	return CalcReflectedLuminance_Direct(surface, viewDir) +
		   CalcReflectedLuminance_Indirect(surface, viewDir
#ifdef PARAM_DDGIGPUScene
			, ddgiSampleContext, indirectMultiplier
#endif // PARAM_DDGIGPUScene
			);
}

#endif // PARAM_GlobalLightsParams
