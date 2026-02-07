#include "SculptorShader.hlsli"

[[descriptor_set(RenderSceneDS)]]
[[descriptor_set(RenderViewDS)]]

[[shader_params(StochasticDIInitialSamplingConstants, u_constants)]]

[[shader_struct(EmissiveMaterialData)]]

#include "RayTracing/RayTracingHelpers.hlsli"
#include "Utils/Random.hlsli"
#include "SceneRendering/UGB.hlsli"
#include "Utils/GBuffer/GBuffer.hlsli"
#include "Materials/MaterialSystem.hlsli"
#include "Shading/Shading.hlsli"
#include "SpecularReflections/RTGICommon.hlsli"


struct EmissiveSample
{
	float3 position;
	float3 normal;
	float3 luminance;
	float pdf_A;
};


EmissiveSample GenerateSample(RngState rngState, EmissivesSampler sampler)
{
	EmissiveSample sample;

	const uint elementIdx = rngState.Next() * sampler.emissivesNum;
	float pdf_A = 1.f / sampler.emissivesNum;

	const EmissiveElement emissiveElem = sampler.emissives.Load(elementIdx);
	const SubmeshGPUData submeshData = emissiveElem.submeshPtr.Load();

	const uint trianglesNum = submeshData.indicesNum / 3u;

	const uint triangleIdx = rngState.Next() * trianglesNum;
	pdf_A *= 1.f / trianglesNum;

	const float2 barycentricCoords = float2(rngState.Next(), rngState.Next());

	const uint index0 = UGB().LoadVertexIndex(submeshData.indicesOffset, triangleIdx * 3u + 0u);
	const uint index1 = UGB().LoadVertexIndex(submeshData.indicesOffset, triangleIdx * 3u + 1u);
	const uint index2 = UGB().LoadVertexIndex(submeshData.indicesOffset, triangleIdx * 3u + 2u);

	const float3 v0 = UGB().LoadLocation(submeshData.locationsOffset, index0);
	const float3 v1 = UGB().LoadLocation(submeshData.locationsOffset, index1);
	const float3 v2 = UGB().LoadLocation(submeshData.locationsOffset, index2);

	const float area = length(cross(v1 - v0, v2 - v0)) * 0.5f;
	pdf_A /= area;

	const float u = 1.f - sqrt(barycentricCoords.x);
	const float v = barycentricCoords.y * sqrt(barycentricCoords.x);
	const float3 locationLocal = u * v0 + v * v1 + (1.f - u - v) * v2;

	sample.position = mul(emissiveElem.entityPtr.Load().transform, float4(locationLocal, 1.f)).xyz;

	const float3 n0 = UGB().LoadNormal(submeshData.normalsOffset, index0);
	const float3 n1 = UGB().LoadNormal(submeshData.normalsOffset, index1);
	const float3 n2 = UGB().LoadNormal(submeshData.normalsOffset, index2);

	sample.normal = normalize(u * n0 + v * n1 + (1.f - u - v) * n2);

	const float2 uv0 = UGB().LoadNormalizedUV(submeshData.uvsOffset, index0);
	const float2 uv1 = UGB().LoadNormalizedUV(submeshData.uvsOffset, index1);
	const float2 uv2 = UGB().LoadNormalizedUV(submeshData.uvsOffset, index2);
	const float2 uv = u * uv0 + v * uv1 + (1.f - u - v) * uv2;

	const EmissiveMaterialData materialData = LoadMaterialFeature<EmissiveMaterialData>(emissiveElem.materialDataHandle, emissiveElem.emissiveMatFeatureID);

	sample.luminance = materialData.emissiveFactor;
	if (materialData.emissiveTexture.IsValid())
	{
		const float3 textureEmissive = materialData.emissiveTexture.SPT_MATERIAL_SAMPLE(BindlessSamplers::LinearClampEdge(), uv).rgb;
		sample.luminance *= textureEmissive;
	}

	sample.pdf_A = pdf_A;

	return sample;
}


/*
 * Samples are generated uniformly on the area of the emissive geometry.
 * This means that pdf is in area measure. When we evaluate sample contribution, this needs to be converted to solid angle measure
 * Based on https://pbr-book.org/4ed/Monte_Carlo_Integration/Transforming_between_Distributions, in order to do this conversion, we need to multiply area pdf by inverse of dw/dA
 * dw/dA = cos(theta) / r^2 (as described in https://pbr-book.org/4ed/Radiometry,_Spectra,_and_Color/Working_with_Radiometric_Integrals)
 * therefore:
 *
 * pdf_w = pdf_A * (dw/dA)^(-1) = pdf_A * r^2 / cos(theta)
 *
 * With such sample distribution Light Transport Equation requires additional Geometric term which is defined in PBRT in https://pbr-book.org/4ed/Light_Transport_I_Surface_Reflection/The_Light_Transport_Equation as jacobian,
 * that relates solid angle to area measure therefore it's based on dw/dA
 *
 * G = dw/dA = cos(theta) / r^2
 *
 * In case of PBRT, Geometry term additionally include visiblity term V which is necessary when samples are generated on the area of the light source,
 * but in our case, it's skipped to be able to compute unshadowed path contribution. Additionally, we also don't include NoL in the geometric term
 * So essentially, GeometricTerm should be the same as the one used in Bitterli paper: https://research.nvidia.com/sites/default/files/pubs/2020-07_Spatiotemporal-reservoir-resampling/ReSTIR.pdf
 */
float AreaToSolidAnglePDF(float pdf_A, float3 sampleNormal, float3 toSample)
{
	const float cosTheta = dot(sampleNormal, normalize(-toSample));
	const float r2 = dot(toSample, toSample);
	return cosTheta > 0.f ? pdf_A * r2 / cosTheta : 0.f;
}


float GeometricTermNoVisibility(float3 sampleNormal, float3 toSample)
{
	const float cosTheta = dot(sampleNormal, normalize(-toSample));
	const float r2 = dot(toSample, toSample);
	return max(cosTheta / r2, 0.f);
}


struct DIReservoir
{
	EmissiveSample sample;
	uint16_t   M;
	float      weightSum;
	float      selected_p_hat;

	static DIReservoir Create()
	{
		DIReservoir reservoir;
		reservoir.sample.position  = 0.f;
		reservoir.sample.normal    = 0.f;
		reservoir.sample.luminance = 0.f;
		reservoir.sample.pdf_A     = 0.f;
		reservoir.M                = 0u;
		reservoir.weightSum        = 0.f;
		reservoir.selected_p_hat   = 0.f;

		return reservoir;
	}

	bool IsValid()
	{
		return selected_p_hat > 0.f;
	}

	bool Update(in float rnd, in EmissiveSample newSample, in float p_hat)
	{
		const float w_i = p_hat / newSample.pdf_A;

		weightSum += w_i;
		M += 1u;

		const bool updateReservoir = (rnd * weightSum < w_i);

		if(updateReservoir)
		{
			sample         = newSample;
			selected_p_hat = p_hat;
		}

		return updateReservoir;
	}

	float NormalizedWeight()
	{
		return selected_p_hat > 0.f ? (1.f / selected_p_hat) * weightSum * (1.f / M) : 0.f;
	}
};


float3 UnshadowedPathContribution(in SurfaceInfo surface, in float3 V, in EmissiveSample sample)
{
	float3 Li = 0.f;

	const float3 toSample = sample.position - surface.location;

	const float G = GeometricTermNoVisibility(sample.normal, toSample);

	if (G < 1e-6f)
	{
		return Li;
	}

	const float3 L = normalize(toSample);

	const float NdotL = max(dot(surface.normal, L), 0.f);

	const RTBRDF brdf = RT_EvaluateBRDF(surface.normal, V, L, surface.roughness, surface.specularColor, surface.diffuseColor);
	Li = sample.luminance * (brdf.diffuse + brdf.specular) * NdotL * G;

	return Li;
}


[shader("raygeneration")]
void InitialSamplingRTG()
{
	const uint2 coords = DispatchRaysIndex().xy;

	const GBufferInterface gBuffer = GBufferInterface(u_constants.gBuffer);

	const SurfaceInfo surface = gBuffer.GetSurfaceInfo(coords);

	const float3 V = normalize(u_sceneView.viewLocation - surface.location);

	RngState rngState = RngState::Create(coords, u_renderSceneConstants.gpuScene.frameIdx);

	DIReservoir reservoir = DIReservoir::Create();

	for (uint i = 0u; i < 10u; ++i)
	{
		const EmissiveSample sample = GenerateSample(rngState, u_constants.emissivesSampler);

		const float p_hat = Luminance(UnshadowedPathContribution(surface, V, sample));

		reservoir.Update(rngState.Next(), sample, p_hat);
	}

	float3 luminance = 0.f;
	if (reservoir.IsValid())
	{
		const float3 toSample = reservoir.sample.position - surface.location;

		RayDesc rayDesc;
		rayDesc.TMin      = 0.01f;
		rayDesc.TMax      = length(toSample) - 0.02f;
		rayDesc.Origin    = surface.location + surface.normal * 0.01f;
		rayDesc.Direction = normalize(toSample);
		const bool isVsible = RTScene().VisibilityTest(rayDesc);

		if (isVsible)
		{
			luminance = UnshadowedPathContribution(surface, V, reservoir.sample) * reservoir.NormalizedWeight();
		}

		if (any(isnan(luminance)))
		{
			luminance = 0.f;
		}
	}

	const float3 prev = u_constants.outputLuminance.Load(coords).rgb;
	u_constants.outputLuminance.Store(coords, float4(prev + LuminanceToExposedLuminance(luminance), 1.f));
}
