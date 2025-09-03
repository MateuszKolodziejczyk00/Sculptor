#ifndef RTGI_RAYS_GENERATION_HLSLI
#define RTGI_RAYS_GENERATION_HLSLI

#include "SpecularReflections/RTGICommon.hlsli"

#include "Shading/Shading.hlsli"
#include "Utils/Random.hlsli"

#include "SpecularReflections/RTGBuffer.hlsli"

#include "SpecularReflections/RTGIRayPayload.hlsli"

#define SPT_RAY_TYPE_SPECULAR 0
#define SPT_RAY_TYPE_DIFFUSE  1

// Based on GPU Gems 2, Chapter 32.3.2.1, "Accurate Real-Time Specular Reflections with Radiance Caching"
float3 EnvBRDFApprox(in float3 specularColor, in float alpha, in float NdotV)
{
	const float2x2 a = float2x2(0.99044f, -1.28514f,
	                            1.29678f, -0.755907f);

	const float3x3 b = float3x3(1.f,      2.92338f,  59.418f,
	                            20.3225f, -27.0302f, 222.592f,
	                            121.563f, 626.13f,   316.627f);

	const float2x2 c = float2x2(0.0365463f, 3.32707f,
	                            9.0632f,    -9.04756f);

	const float3x3 d = float3x3(1.f,      3.59685f,  -1.36772f,
	                            9.04401f, -16.3174f, 9.22949f,
	                            5.56589f, 19.7886f,  -20.2123f);

	const float num0   = mul(mul(float2(1.f, alpha), a), float2(1.f, NdotV));
	const float denom0 = mul(mul(float3(1.f, alpha, Pow3(alpha)), b), float3(1.f, NdotV, Pow3(NdotV)));

	const float num1   = mul(mul(float2(1.f, alpha), c), float2(1.f, NdotV));
	const float denom1 = mul(mul(float3(1.f, alpha, Pow3(alpha)), d), float3(1.f, NdotV, Pow3(NdotV)));

	return (num0 / denom0) + (num1 / denom1) * specularColor;
}


// Based on GPU Zen 3, Chapter 7.9.6. "Evolution of Real-Time Lighting Pipeline in Cyberpunk 2077"
float EstimateDiffuseProbability(in float3 specularColor, in float3 diffuseColor, in float roughness, in float NdotV)
{
	if (roughness < SPECULAR_TRACE_MAX_ROUGHNESS)
	{
		return 0.f;
	}

	const float3 envBRDF = EnvBRDFApprox(specularColor, RoughnessToAlpha(roughness), NdotV);

	const float specLum = Luminance(envBRDF);
	const float diffLum = Luminance(diffuseColor * (1.f - envBRDF));
	const float totalLum = specLum + diffLum;

	float diffuseProbability = totalLum > 0.1f ? diffLum / totalLum : 1.f;
	
	if (diffuseProbability > 0.f && diffuseProbability < 1.f)
	{
		diffuseProbability = clamp(diffuseProbability, 0.05f, 0.95f);
	}

	return diffuseProbability;
}


struct RayDirectionInfo
{
	float3 direction;
	float  pdf;
	uint   rayType; // SPT_RAY_TYPE_SPECULAR or SPT_RAY_TYPE_DIFFUSE
};


RayDirectionInfo GenerateReflectionRayDir(in float3 diffuseColor, in float3 specularColor, in float3 normal, in float roughness, in float3 toView, inout RngState rng)
{
	float3 direction;

	const float diffuseProbability = EstimateDiffuseProbability(specularColor, diffuseColor, roughness, dot(normal, toView));

	bool isDiffuse = rng.Next() < diffuseProbability;

	float diffusePdf  = 0.f;
	float specularPdf = 0.f;

	const float alpha = RoughnessToAlpha(roughness);

	if(isDiffuse)
	{
		const float2 noise = float2(rng.Next(), rng.Next());

		const float3 tangent   = abs(dot(normal, UP_VECTOR)) > 0.9f ? cross(normal, RIGHT_VECTOR) : cross(normal, UP_VECTOR);
		const float3 bitangent = cross(normal, tangent);
		const float3x3 tangentSpace = transpose(float3x3(tangent, bitangent, normal));

		direction   = RandomVectorInUniformHemisphere(tangentSpace, noise, OUT diffusePdf);
		specularPdf = PDFVNDFIsotrpic(toView, direction, alpha, normal);
	}
	else
	{
		if (roughness < SPECULAR_TRACE_MAX_ROUGHNESS)
		{
			direction = reflect(-toView, normal);
			specularPdf = -1.f; // -1 means specular trace
			diffusePdf = 0.f;
		}
		else
		{
			uint attempt = 0u;

			while (true)
			{
				const float2 noise = float2(rng.Next(), rng.Next());

				const float3 h = SampleVNDFIsotropic(noise, toView, alpha, normal);

				const float3 reflectedRay = reflect(-toView, h);

				if (dot(reflectedRay, normal) > 0.f)
				{
					specularPdf = PDFVNDFIsotrpic(toView, reflectedRay, alpha, normal);
					direction = reflectedRay;

					break;
				}
				else if (attempt == 16u)
				{
					direction = 0.f;
					specularPdf = 0.f;

					break;
				}

				++attempt;
			}

			diffusePdf = PDFHemisphereUniform(normal, direction);
		}
	}

	// Apply the balance heuristic
	const float finalPDF = diffusePdf * diffuseProbability + specularPdf * (1.f - diffuseProbability);

	RayDirectionInfo result;
	result.direction = direction;
	result.pdf       = finalPDF;
	result.rayType   = isDiffuse ? SPT_RAY_TYPE_DIFFUSE : SPT_RAY_TYPE_SPECULAR;

	return result;
}


RayDirectionInfo GenerateReflectionRayDir(in float4 baseColorMetallic, in float3 normal, in float roughness, in float3 toView, inout RngState rng)
{
	float3 specularColor;
	float3 diffuseColor;
	ComputeSurfaceColor(baseColorMetallic.rgb, baseColorMetallic.w, OUT diffuseColor, OUT specularColor);

	return GenerateReflectionRayDir(diffuseColor, specularColor, normal, roughness, toView, rng);
}


RayHitResult RTGITraceRay(RaytracingAccelerationStructure tlas, in float3 rayOrigin, in float3 rayDirection)
{
	const float maxHitDistance = 200.f;

	RayDesc rayDesc;
	rayDesc.TMin      = 0.01f;
	rayDesc.TMax      = maxHitDistance;
	rayDesc.Origin    = rayOrigin;
	rayDesc.Direction = rayDirection;

	RTGIRayPayload payload;

	TraceRay(tlas,
			 0,
			 0xFF,
			 0,
			 1,
			 0,
			 rayDesc,
			 payload);

	const bool isBackface = payload.distance < 0.f;
	const bool isValidHit = !isBackface && payload.distance < maxHitDistance;

	RayHitResult hitResult = RayHitResult::CreateEmpty();

	if (isValidHit)
	{
		const float4 baseColorMetallic = UnpackFloat4x8(payload.baseColorMetallic);

		hitResult.normal      = normalize(payload.normal);
		hitResult.roughness   = payload.roughness;
		hitResult.baseColor   = baseColorMetallic.rgb;
		hitResult.metallic    = baseColorMetallic.w;
		hitResult.hitType     = RTGBUFFER_HIT_TYPE_VALID_HIT;
		hitResult.hitDistance = payload.distance;
		hitResult.emissive    = payload.emissive;
	}
	else if (!isBackface)
	{
		hitResult.hitType = RTGBUFFER_HIT_TYPE_NO_HIT;
	}
	else
	{
		hitResult.hitType = RTGBUFFER_HIT_TYPE_BACKFACE;
	}

	return hitResult;
}

#endif // RTGI_RAYS_GENERATION_HLSLI