#ifndef SHADING_HLSLI
#define SHADING_HLSLI

#include "Shading/BRDFs/Diffuse.hlsli"
#include "Shading/BRDFs/Specular.hlsli"

typealias DiffuseBRDF   = EONDiffuseBRDF;
typealias SpecularBRDF  = GGXSpecularBRDF<true, true>;
typealias ClearCoatBRDF = GGXClearCoatBRDF;


struct LightingContribution
{
	static LightingContribution Create(in float3 inSceneLuminance, in float3 inEyeAdaptationLuminance)
	{
		LightingContribution res;
		res.sceneLuminance         = inSceneLuminance;
		res.eyeAdaptationLuminance = inEyeAdaptationLuminance;
		return res;
	}

	static LightingContribution Create(in float3 inLuminance)
	{
		return Create(inLuminance, inLuminance);
	}

	float3 sceneLuminance;
	float3 eyeAdaptationLuminance;
};


LightingContribution operator*(LightingContribution lhs, float rhs)
{
	LightingContribution res;
	res.sceneLuminance         = lhs.sceneLuminance * rhs;
	res.eyeAdaptationLuminance = lhs.eyeAdaptationLuminance * rhs;
	return res;
}


LightingContribution operator*(LightingContribution lhs, float3 rhs)
{
	LightingContribution res;
	res.sceneLuminance         = lhs.sceneLuminance * rhs;
	res.eyeAdaptationLuminance = lhs.eyeAdaptationLuminance * rhs;
	return res;
}


struct BRDFLobes
{
	float3 diffuse;
	float3 specular;
};


interface IClosure
{
	static BRDFLobes Evaluate<let allowDeltaBRDF : bool>(in ShadedSurface surface, in float3 viewDir, in float3 lightDir);

	static LightingContribution EvaluateContribution<let allowDeltaBRDF : bool>(in ShadedSurface surface, in float3 viewDir, in float3 lightDir, in float3 peakIlluminance)
	{
		const BRDFLobes brdf = Evaluate<allowDeltaBRDF>(surface, viewDir, lightDir);

		const float3 sceneLuminance         = (brdf.diffuse + brdf.specular) * peakIlluminance;
		const float3 eyeAdaptationLuminance = brdf_utils::lambert::Evaluate(1.f) * peakIlluminance;

		return LightingContribution::Create(sceneLuminance, eyeAdaptationLuminance);
	}

	static BRDFLobes RTEvaluate(in ShadedSurface surface, in float3 viewDir, in float3 lightDir)
	{
		return Evaluate<true>(surface, viewDir, lightDir);
	}

	static ShadingSample Sample(in ShadedSurface surface, in float3 viewDir, inout RngState rng);
};


struct OpaqueClosure : IClosure
{
	static float RoughnessDeltaThreshold = 0.01f;

	static BRDFLobes Evaluate<let allowDeltaBRDF : bool>(in ShadedSurface surface, in float3 viewDir, in float3 lightDir)
	{
		const ShadingVectors shadingVectors = ShadingVectors(viewDir, lightDir, surface.normal);

		if (allowDeltaBRDF && surface.roughness < RoughnessDeltaThreshold)
		{
			const float3 f = brdf_utils::F_Schlick(surface.f0, shadingVectors.VoH);

			BRDFLobes lobes;
			lobes.specular = f;
			lobes.diffuse  = DiffuseBRDF::Evaluate(surface, shadingVectors) * (1.f - f) * shadingVectors.NoL;
			return lobes;
		}
		else
		{
			const float3 clearCoat = ClearCoatBRDF::Evaluate(surface, shadingVectors) * surface.clearCoatWeight;

			const float3 clearCoatExit  = 1.f -  ClearCoatBRDF::F(surface, shadingVectors.NoV) * surface.clearCoatWeight;
			const float3 clearCoatEnter = 1.f -  ClearCoatBRDF::F(surface, shadingVectors.NoL) * surface.clearCoatWeight;
			float3 clearCoatThroughput = clearCoatExit * clearCoatEnter;

			if (surface.clearCoatWeight > 0.f)
			{
				const float3 darkening = lerp(1.f, ClearCoatBRDF::ComputeDarkeningFactor(surface, shadingVectors), surface.clearCoatWeight);
				clearCoatThroughput *= darkening;
			}

			float3 specularThroughput;
			const float3 specular = SpecularBRDF::Evaluate(surface, shadingVectors, OUT specularThroughput) * clearCoatThroughput;
			const float3 diffuse = DiffuseBRDF::Evaluate(surface, shadingVectors) * specularThroughput * clearCoatThroughput;

			BRDFLobes lobes;
			lobes.diffuse = diffuse * shadingVectors.NoL;
			lobes.specular = (specular + clearCoat) * shadingVectors.NoL;

			return lobes;
		}
	}

	static ShadingSample Sample(in ShadedSurface surface, in float3 viewDir, inout RngState rng)
	{
		float3 direction;

		const LobeProbabilities lobeProbabilites = EstimateLobeProbabilities(surface, dot(surface.normal, viewDir));

		const float lobeRng = rng.Next();

		bool isDiffuse = false, isSpecular = false, isClearCoat = false;
		if (lobeRng < lobeProbabilites.diffuse)
		{
			isDiffuse = true;
		}
		else if (lobeRng < lobeProbabilites.diffuse + lobeProbabilites.specular)
		{
			isSpecular = true;
		}
		else
		{
			isClearCoat = true;
		}

		float diffusePdf  = 0.f;
		float specularPdf = 0.f;
		float clearCoatPdf = 0.f;

		const float alpha = RoughnessToAlpha(surface.roughness);

		if(isDiffuse)
		{
			const float2 noise = float2(rng.Next(), rng.Next());

			const ShadingSample sample = DiffuseBRDF::Sample(surface.normal, surface.roughness, viewDir, noise);
			direction  = sample.direction;
			diffusePdf = sample.pdf;

			specularPdf = SpecularBRDF::PDF(surface.normal, surface.roughness, viewDir, direction);
			clearCoatPdf = ClearCoatBRDF::PDF(surface.normal, surface.clearCoatRoughness, viewDir, direction);
		}
		else if(isSpecular)
		{
			if (surface.roughness < RoughnessDeltaThreshold)
			{
				direction = reflect(-viewDir, surface.normal);
				specularPdf  = -1.f; // -1 means specular trace
				diffusePdf   = 0.f;
				clearCoatPdf = 0.f;
			}
			else
			{
				const float2 noise = float2(rng.Next(), rng.Next());

				const ShadingSample sample = SpecularBRDF::Sample(surface.normal, surface.roughness, viewDir, noise);

				direction    = sample.direction;
				specularPdf  = sample.pdf;
				diffusePdf   = DiffuseBRDF::PDF(surface.normal, surface.roughness, viewDir, direction);
				clearCoatPdf = ClearCoatBRDF::PDF(surface.normal, surface.clearCoatRoughness, viewDir, direction);
			}
		}
		else if(isClearCoat)
		{
			const float2 noise = float2(rng.Next(), rng.Next());

			const ShadingSample sample = ClearCoatBRDF::Sample(surface.normal, surface.clearCoatRoughness, viewDir, noise);

			direction = sample.direction;
			clearCoatPdf = sample.pdf;

			diffusePdf = DiffuseBRDF::PDF(surface.normal, surface.roughness, viewDir, direction);
			specularPdf = SpecularBRDF::PDF(surface.normal, surface.roughness, viewDir, direction);
		}

		// Apply the balance heuristic
		const float finalPDF = diffusePdf * lobeProbabilites.diffuse + specularPdf * lobeProbabilites.specular + clearCoatPdf * lobeProbabilites.clearCoat;

		ShadingSample result;
		result.direction = direction;
		result.pdf       = finalPDF;

		return result;
	}

	static LobeProbabilities EstimateLobeProbabilities(in ShadedSurface surface, in float NdotV)
	{
		if (surface.roughness < RoughnessDeltaThreshold)
		{
			LobeProbabilities res;
			res.diffuse = 0.f;
			res.specular = 1.f;
			res.clearCoat = 0.f;
	
			return res;
		}
	
		const float3 envBRDF = EnvBRDFApprox(surface.f0, RoughnessToAlpha(surface.roughness), NdotV);
		const float3 clearCoatEnvBRDF = EnvBRDFApprox(brdf_utils::clearCoatF0, RoughnessToAlpha(surface.clearCoatRoughness), NdotV) * surface.clearCoatWeight;
	
		const float3 clearCoatThroughput = 1.f - clearCoatEnvBRDF;
		const float3 specularThroughput = (1.f - envBRDF);
	
		const float clearCoatLum = Luminance(clearCoatEnvBRDF);
		const float specLum = Luminance(envBRDF * clearCoatThroughput);
		const float diffLum = Luminance(surface.albedo * specularThroughput * clearCoatThroughput);
		const float totalLum = specLum + diffLum + clearCoatLum;
	
		if (totalLum <= 0.f)
		{
			LobeProbabilities res;
			res.diffuse = 1.f;
			res.specular = 0.f;
			res.clearCoat = 0.f;
	
			return res;
		}
	
		const float diffuseProbability   = clamp(diffLum / totalLum, 0.05f, 0.9f);
		const float clearCoatProbability = surface.clearCoatWeight > 0.f ? clamp(clearCoatLum / totalLum, 0.05f, 0.9f) : 0.f;
		const float specularProbability  = 1.f - diffuseProbability - clearCoatProbability;
	
		LobeProbabilities res;
		res.diffuse   = diffuseProbability;
		res.specular  = specularProbability;
		res.clearCoat = clearCoatProbability;
	
		return res;
	}
};


LightingContribution DoShading(in ShadedSurface surface, in float3 lightDir, in float3 viewDir, in float3 peakIlluminance)
{
	return OpaqueClosure::EvaluateContribution<false>(surface, lightDir, viewDir, peakIlluminance);
}

typealias RTClosure = OpaqueClosure;

#endif // SHADING_HLSLI
