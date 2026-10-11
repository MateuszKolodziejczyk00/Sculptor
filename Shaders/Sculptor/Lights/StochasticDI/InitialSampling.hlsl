#include "SculptorShader.hlsli"

[[shader_params(RenderSceneConstants, SCENE)]]
[[shader_params(GPURenderView, VIEW)]]

[[shader_params(StochasticDIInitialSamplingConstants, CONSTS)]]

[[shader_struct(EmissiveMaterialData)]]

#include "RayTracing/RayTracingHelpers.hlsli"
#include "SceneRendering/UGB.hlsli"
#include "Utils/GBuffer/GBuffer.hlsli"
#include "Materials/MaterialSystem.hlsli"
#include "Utils/Random.hlsli"
#include "Lights/StochasticDI/StochasticDI.hlsli"


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

	float3 v0 = UGB().LoadLocation(submeshData.locationsOffset, index0);
	float3 v1 = UGB().LoadLocation(submeshData.locationsOffset, index1);
	float3 v2 = UGB().LoadLocation(submeshData.locationsOffset, index2);
	v0 = mul(emissiveElem.entityPtr.Load().transform, float4(v0, 1.f)).xyz;
	v1 = mul(emissiveElem.entityPtr.Load().transform, float4(v1, 1.f)).xyz;
	v2 = mul(emissiveElem.entityPtr.Load().transform, float4(v2, 1.f)).xyz;

	const float area = length(cross(v1 - v0, v2 - v0)) * 0.5f;
	pdf_A *= 1.f / area;

	const float u = 1.f - sqrt(barycentricCoords.x);
	const float v = barycentricCoords.y * sqrt(barycentricCoords.x);
	const float3 sampleLocation = u * v0 + v * v1 + (1.f - u - v) * v2;

	sample.location = sampleLocation;

	const float3 n0 = UGB().LoadNormal(submeshData.normalsOffset, index0);
	const float3 n1 = UGB().LoadNormal(submeshData.normalsOffset, index1);
	const float3 n2 = UGB().LoadNormal(submeshData.normalsOffset, index2);

	const float3 normalLocal = normalize(u * n0 + v * n1 + (1.f - u - v) * n2);
	sample.normal = mul((float3x3)emissiveElem.entityPtr.Load().transform, normalLocal);

	const float2 uv0 = UGB().LoadNormalizedUV(submeshData.uvsOffset, index0);
	const float2 uv1 = UGB().LoadNormalizedUV(submeshData.uvsOffset, index1);
	const float2 uv2 = UGB().LoadNormalizedUV(submeshData.uvsOffset, index2);
	const float2 uv = u * uv0 + v * uv1 + (1.f - u - v) * uv2;

	const EmissiveMaterialData materialData = LoadMaterialFeature<EmissiveMaterialData>(emissiveElem.materialDataHandle, emissiveElem.emissiveMatFeatureID);

	sample.luminance = materialData.emissiveFactor;
	if (materialData.emissiveTexture.IsValid())
	{
		DefaultMaterialSampler sampler;
		const float3 textureEmissive = sampler.SPT_MATERIAL_SAMPLE(materialData.emissiveTexture, SPT_SAMPLER_ANISO_IF_NOT_EXPLICIT_LEVEL, uv).rgb;
		sample.luminance *= textureEmissive;
	}

	sample.pdf_A = pdf_A;

	return sample;
}


[shader("raygeneration")]
void InitialSamplingRTG()
{
	const uint2 coords = DispatchRaysIndex().xy;

	const GBufferInterface gBuffer = GBufferInterface(CONSTS->gBuffer);

	const SurfaceInfo surface = gBuffer.GetSurfaceInfo(coords);

	const float3 V = normalize(VIEW->sceneView.viewLocation - surface.location);

	RngState rngState = RngState::Create(coords, SCENE->gpuScene.frameIdx);

	DIReservoir reservoir = DIReservoir::Create();

	for (uint i = 0u; i < CONSTS->risSamplesNum; ++i)
	{
		const EmissiveSample sample = GenerateSample(rngState, CONSTS->emissivesSampler);

		const float p_hat = Luminance(UnshadowedPathContribution(surface, V, sample));

		reservoir.Update(rngState.Next(), sample, p_hat);
	}

	if (reservoir.IsValid())
	{
		const float ssLenght = CONSTS->ssrtRange;
		if (!PerformDIVisibilityTest(CONSTS->ssTracer, VIEW->sceneView, surface, reservoir.sample, ssLenght, rngState.Next()))
		{
			reservoir.weightSum = 0.f;
		}
	}

	if (CONSTS->historyReservoirs.IsValid() && CONSTS->enableTemporalResampling)
	{
		const float2 uv = (float2(coords) + 0.5f) * CONSTS->gBuffer.pixelSize;

		const float2 motion = CONSTS->motion.Load(uint3(coords, 0u));
		float2 reprojectedUV = uv - motion;
		if (all(saturate(reprojectedUV) == reprojectedUV))
		{
			const uint2 reprojectedCoords = uint2(reprojectedUV * CONSTS->gBuffer.resolution);

			const float historyDepth = CONSTS->historyDepth.Load(reprojectedCoords);
			const float3 historyLocation = NDCToWorldSpace(float3(reprojectedUV * 2.f - 1.f, historyDepth), VIEW->prevFrameSceneView);
			const float3 historyNormal   = OctahedronDecodeNormal(CONSTS->historyNormal.Load(reprojectedCoords));

			if (CanResampleSurface(surface, historyLocation, historyNormal))
			{
				const uint historyReservoirIdx = GetScreenReservoirIdx(reprojectedCoords, CONSTS->reservoirsResolution);

				DIReservoir historyReservoir = UnpackDIReservoir(CONSTS->historyReservoirs.Load(historyReservoirIdx));

				if (historyReservoir.IsValid() && historyReservoir.age < 16u)
				{
					float p_hat = Luminance(UnshadowedPathContribution(surface, V, historyReservoir.sample));
					if (any(isnan(p_hat)))
					{
						p_hat = 0.f;
					}

					reservoir.Update(rngState.Next(), historyReservoir, p_hat);
					reservoir.age = historyReservoir.age;
				}
			}
		}
	}

	reservoir.Normalize();
	reservoir.age += 1u;
	reservoir.M = 1u;

	const uint reservoirIdx = GetScreenReservoirIdx(coords, CONSTS->reservoirsResolution);
	CONSTS->outReservoirs.Store(reservoirIdx, PackDIReservoir(reservoir));
}
