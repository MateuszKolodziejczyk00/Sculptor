#ifndef EMISSIVE_SAMPLING_HLSLI
#define EMISSIVE_SAMPLING_HLSLI

[[shader_struct(EmissiveMaterialData)]]

#include "SceneRendering/GPUScene.hlsli"
#include "SceneRendering/UGB.hlsli"
#include "Materials/MaterialSystem.hlsli"
#include "Utils/Random.hlsli"
#include "Utils/Packing.hlsli"


PackedEmissiveSample GenerateEmissiveSample(inout RngState rngState, in EmissivesSampler sampler)
{
	const uint elementIdx = rngState.Next() * sampler.emissivesNum;
	float pdf_A = 1.f / sampler.emissivesNum;

	const EmissiveElement emissiveElem = sampler.emissives.Load(elementIdx);
	const SubmeshGPUData submeshData = emissiveElem.submeshPtr.Load();
	const float4x4 transform = emissiveElem.entityPtr.Load().transform;

	const uint trianglesNum = submeshData.indicesNum / 3u;

	const uint triangleIdx = rngState.Next() * trianglesNum;
	pdf_A *= 1.f / trianglesNum;

	const float2 barycentricCoords = float2(rngState.Next(), rngState.Next());

	const uint index0 = UGB().LoadVertexIndex(submeshData.indicesOffset, triangleIdx * 3u + 0u);
	const uint index1 = UGB().LoadVertexIndex(submeshData.indicesOffset, triangleIdx * 3u + 1u);
	const uint index2 = UGB().LoadVertexIndex(submeshData.indicesOffset, triangleIdx * 3u + 2u);

	const float3 v0 = mul(transform, float4(UGB().LoadLocation(submeshData.locationsOffset, index0), 1.f)).xyz;
	const float3 v1 = mul(transform, float4(UGB().LoadLocation(submeshData.locationsOffset, index1), 1.f)).xyz;
	const float3 v2 = mul(transform, float4(UGB().LoadLocation(submeshData.locationsOffset, index2), 1.f)).xyz;

	const float area = length(cross(v1 - v0, v2 - v0)) * 0.5f;
	pdf_A *= 1.f / area;

	const float u = 1.f - sqrt(barycentricCoords.x);
	const float v = barycentricCoords.y * sqrt(barycentricCoords.x);

	const float3 n0 = UGB().LoadNormal(submeshData.normalsOffset, index0);
	const float3 n1 = UGB().LoadNormal(submeshData.normalsOffset, index1);
	const float3 n2 = UGB().LoadNormal(submeshData.normalsOffset, index2);
	const float3 normalLocal = normalize(u * n0 + v * n1 + (1.f - u - v) * n2);

	const float2 uv0 = UGB().LoadNormalizedUV(submeshData.uvsOffset, index0);
	const float2 uv1 = UGB().LoadNormalizedUV(submeshData.uvsOffset, index1);
	const float2 uv2 = UGB().LoadNormalizedUV(submeshData.uvsOffset, index2);
	const float2 uv = u * uv0 + v * uv1 + (1.f - u - v) * uv2;

	const EmissiveMaterialData materialData = LoadMaterialFeature<EmissiveMaterialData>(emissiveElem.materialDataHandle, emissiveElem.emissiveMatFeatureID);

	float3 luminance = materialData.emissiveFactor;
	if (materialData.emissiveTexture.IsValid())
	{
		DefaultMaterialSampler materialSampler;
		luminance *= materialSampler.SampleLevel(materialData.emissiveTexture, BindlessSamplers::LinearRepeat(), uv, 0.f).rgb;
	}

	PackedEmissiveSample sample;
	sample.location        = u * v0 + v * v1 + (1.f - u - v) * v2;
	sample.packedLuminance = EncodeRGBToLogLuv(luminance);
	sample.normal          = OctahedronEncodeNormal(normalize(mul((float3x3)transform, normalLocal)));
	sample.pdf_A           = pdf_A;

	return sample;
}

#endif // EMISSIVE_SAMPLING_HLSLI
