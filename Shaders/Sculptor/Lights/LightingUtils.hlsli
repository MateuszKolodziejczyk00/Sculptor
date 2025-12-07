#ifndef LIGHTING_UTILS_H
#define LIGHTING_UTILS_H

#include "Shading/Shading.hlsli"


#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_SPOT  1


class SceneLightingAccumulator
{
	static SceneLightingAccumulator Create()
	{
		SceneLightingAccumulator accumulator;
		accumulator.sceneLuminance = 0.f;
		return accumulator;
	}

	void Accumulate(in LightingContribution contribution)
	{
		sceneLuminance += contribution.sceneLuminance;
	}

	void Accumulate(in float3 luminance)
	{
		sceneLuminance += luminance;
	}

	float3 GetLuminance()
	{
		return sceneLuminance;
	}

	float3 sceneLuminance;
};


class ViewLightingAccumulator
{
	static ViewLightingAccumulator Create()
	{
		ViewLightingAccumulator accumulator;
		accumulator.sceneLuminance         = 0.f;
		accumulator.eyeAdaptationLuminance = 0.f;
		return accumulator;
	}

	void Accumulate(in LightingContribution contribution)
	{
		sceneLuminance         += contribution.sceneLuminance;
		eyeAdaptationLuminance += contribution.eyeAdaptationLuminance;
	}

	float3 GetLuminance()
	{
		return sceneLuminance;
	}

	float3 GetEyeAdaptationLuminance()
	{
		return eyeAdaptationLuminance;
	}

	float3 sceneLuminance;
	float3 eyeAdaptationLuminance;
};


float HenyeyGreensteinPhaseFunction(in float g, in float cosTheta)
{
	const float numerator = 1.f - g * g;
	const float denominator = 4.f * PI * pow(1.f + g * g - 2.f * g * cosTheta, 1.5f);
	return numerator / denominator;
}


float PhaseFunction(in float3 toView, in float3 fromLight, float g)
{
	const float cosTheta = dot(toView, fromLight);
	return HenyeyGreensteinPhaseFunction(g, cosTheta);
}


[[shader_struct(LocalLightGPUData)]]

[[override]]
struct LocalLightInterface : LocalLightGPUData
{
	bool IsPointLight()
	{
		return type == LIGHT_TYPE_POINT;
	}

	bool IsSpotLight()
	{
		return type == LIGHT_TYPE_SPOT;
	}
};


// Clamped inverse-square falloff function from:
// Real Shading in Unreal Engine 4 by Brian Karis
float ComputeLightDistanceAttenuationAtLocation(LocalLightInterface localLight, float3 location)
{
#if VOLUMETRIC_FOG_LIGHTING
	// Increase bias for spot lights because they can cause some aliasing in volumetric fog
	const float biasMeters = localLight.type == LIGHT_TYPE_SPOT ? 0.1f : 0.01f;
#else
	const float biasMeters = 0.01f;
#endif // VOLUMETRIC_FOG_LIGHTING

	const float dist2 = Dist2(localLight.location, location);
	return saturate(Pow2(1.f - Pow2(dist2) / Pow4(localLight.range))) / (dist2 + biasMeters);
}


float ComputeLightAngularAttenuationAtLocation(LocalLightInterface localLight, float3 location)
{
	SPT_CHECK(localLight.type == LIGHT_TYPE_SPOT);

	const float3 lightDir = normalize(location - localLight.location);
	const float cd = dot(lightDir, localLight.direction);

	const float attenuation = Pow2(saturate(cd * localLight.lightAngleScale + localLight.lightAngleOffset));

	return attenuation;
}


float ComputeLightLuminousIntensity(LocalLightInterface localLight)
{
	if (localLight.IsPointLight())
	{
		return localLight.luminousPower / (4.f * PI);
	}
	else if (localLight.IsSpotLight())
	{
		// In theory this should be based on cone angle, but this could be confusing to setup
		// Because of that, we use the same formula as proposed by Sebastian Lagarde in "Moving Frostbite to PBR" equation 17
		return localLight.luminousPower / PI;
	}
	else
	{
		return 0.f;
	}
}


float3 GetLightIlluminanceAtLocation(LocalLightInterface localLight, float3 location)
{
	const float luminousIntensity = ComputeLightLuminousIntensity(localLight);
	float attenuation = ComputeLightDistanceAttenuationAtLocation(localLight, location);

	if (localLight.IsSpotLight())
	{
		attenuation *= ComputeLightAngularAttenuationAtLocation(localLight, location);
	}

	if (localLight.iesProfile.IsValid())
	{
		const float3 lightDir = normalize(location - localLight.location);
		const float cosTheta = lightDir.z * 0.5f + 0.5f;
		const float phi = atan2(lightDir.x, lightDir.y) / (2.f * PI) + 0.5f;

		const float iesAttenuation = localLight.iesProfile.SampleLevel(BindlessSamplers::LinearClampEdge(), float2(phi, cosTheta), 0.f);
		attenuation *= iesAttenuation;
	}
	
	return attenuation * localLight.color * luminousIntensity;
}


LightingContribution CalcLighting(ShadedSurface surface, float3 lightDir, float3 viewDir, float3 peakIlluminance)
{
	return DoShading(surface, lightDir, viewDir, peakIlluminance);
}


void CreateLightBoundingSphere(const LocalLightInterface light, out float3 center, out float radius)
{
	if (light.IsPointLight())
	{
		center = light.location;
		radius = light.range;
	}
	else if (light.IsSpotLight())
	{
		if (light.halfAngleCos > 0.f)
		{
			center = light.location + light.direction * light.boundingSphereRadius;
			radius = light.boundingSphereRadius;
		}
		else
		{
			center = light.location + light.direction * light.range;
			radius = light.boundingSphereRadius;
		}
	}
	else
	{
		center = float3(0.f, 0.f, 0.f);
		radius = 0.f;
	}
}

#endif // LIGHTING_UTILS_H
