#include "SculptorShader.hlsli"

[[shader_params(RenderSceneConstants, SCENE)]]
[[shader_params(GPURenderView, VIEW)]]

[[shader_params(DISpatialResamplingConstants , CONSTS)]]

#include "RayTracing/RayTracingHelpers.hlsli"
#include "Lights/StochasticDI/StochasticDI.hlsli"
#include "Utils/Random.hlsli"


// Make sure that seed is same for whole warp, this will increase cache coherence
// Idea from: https://github.com/EmbarkStudios/kajiya/blob/main/assets/shaders/rtdgi/restir_spatial.hlsl
#define WAVE_COHERENT_SPATIAL_RESAMPLING 0


[shader("raygeneration")]
void DISpatialResamplingRTG()
{
	const uint2 coords = DispatchRaysIndex().xy;

	const GBufferInterface gBuffer = GBufferInterface(CONSTS->gBuffer);

	const SurfaceInfo surface = gBuffer.GetSurfaceInfo(coords);

	const float3 V = normalize(VIEW->sceneView.viewLocation - surface.location);

	DIReservoir reservoir = DIReservoir::Create();

	const uint reservoirIdx = GetScreenReservoirIdx(coords, CONSTS->reservoirsResolution);

	// Emissive lighting of these pixels is handled by RT GI
	if (surface.roughness < RTClosure::RoughnessNoResamplingThreshold)
	{
		CONSTS->outReservoirs.Store(reservoirIdx, PackDIReservoir(reservoir));
		CONSTS->rwSpecularHitDist.Store(coords, 0.f);
		CONSTS->rwDiffuse.Store(coords, 0.f);
		CONSTS->rwLightDirection.Store(coords, 0.f);
		return;
	}

	const DIReservoir initialReservoir = UnpackDIReservoir(CONSTS->inReservoirs.Load(reservoirIdx));

	if (initialReservoir.IsValid())
	{
		float p_hat = Luminance(UnshadowedPathContribution(surface, V, initialReservoir.sample));
		if (any(isnan(p_hat)))
		{
			p_hat = 0.f;
		}

		reservoir.Update(0.f, initialReservoir, p_hat);
		reservoir.age = initialReservoir.age;
	}

	RngState rngState = RngState::Create(coords, SCENE->gpuScene.frameIdx + 7u);

	const int range       = CONSTS->spatialResamplingRange;
	const uint samplesNum = CONSTS->spatialSamplesNum;

	for (uint i = 0u; i < samplesNum; ++i)
	{
#if WAVE_COHERENT_SPATIAL_RESAMPLING
		const int2 offset = WaveReadLaneFirst((float2(rngState.Next(), rngState.Next()) * 2.f - 1.f) * range);
#else
		const int2 offset = (float2(rngState.Next(), rngState.Next()) * 2.f - 1.f) * range;
#endif

		int2 sampleCoords = abs(int2(coords) + offset);
		if (sampleCoords.x >= CONSTS->gBuffer.resolution.x)
		{
			sampleCoords.x = 2 * (CONSTS->gBuffer.resolution.x - 1) - sampleCoords.x;
		}
		if (sampleCoords.y >= CONSTS->gBuffer.resolution.y)
		{
			sampleCoords.y = 2 * (CONSTS->gBuffer.resolution.y - 1) - sampleCoords.y;
		}

#if ENABLE_SURFACE_CHECK
		const SurfaceInfo sampleSurface = gBuffer.GetSurfaceInfo(sampleCoords);
		if (!CanResampleSurface(surface, sampleSurface.location, sampleSurface.normal))
		{
			continue;
		}
#endif

		const uint sampleReservoirIdx = GetScreenReservoirIdx(sampleCoords, CONSTS->reservoirsResolution);
		const DIReservoir sampleReservoir = UnpackDIReservoir(CONSTS->inReservoirs.Load(sampleReservoirIdx));

		if (!sampleReservoir.IsValid())
		{
			continue;
		}

		const float p_hat = Luminance(UnshadowedPathContribution(surface, V, sampleReservoir.sample));

		reservoir.Update(rngState.Next(), sampleReservoir, p_hat);
	}

	reservoir.Normalize();

	// Update visibility
	{
		const float ssLenght = CONSTS->ssrtRange;
		if (!PerformDIVisibilityTest(CONSTS->ssTracer, VIEW->sceneView, surface, reservoir.sample, ssLenght, rngState.Next()))
		{
			reservoir.weightSum = 0.f;
		}
	}

	CONSTS->outReservoirs.Store(reservoirIdx, PackDIReservoir(reservoir));

	// Decorrelation
	{
		RngState decorrelationRng = RngState::Create(coords, SCENE->gpuScene.frameIdx + 29278u);

		const float decorrelationProbability = VIEW->settings.stochasticDI.decorrelationProbability;
		if (decorrelationRng.Next() < decorrelationProbability)
		{
			reservoir = UnpackDIReservoir(CONSTS->initialReservoirs.Load(reservoirIdx));
		}
	}

	const StochasticDIShadingResult shadingResult = FinalDIShading(surface, V, reservoir.sample, reservoir.weightSum);

	float3 diffuseLo = shadingResult.diffuse;
	diffuseLo = LuminanceToExposedLuminance(diffuseLo);
	diffuseLo /= max(surface.diffuseColor, SMALL_NUMBER);

	const float NdotV = saturate(dot(surface.normal, V));
	const float2 integratedBRDF = GLOBALS->brdfIntegrationLUT.SampleLevel(BindlessSamplers::LinearClampEdge(), float2(NdotV, surface.roughness), 0);

	float3 specularLo = shadingResult.specular;
	specularLo = LuminanceToExposedLuminance(specularLo);
	specularLo /= max((surface.specularColor * integratedBRDF.x + integratedBRDF.y), 0.01f);

	CONSTS->rwSpecularHitDist.Store(coords, float4(specularLo, shadingResult.hitDistance));
	CONSTS->rwDiffuse.Store(coords, diffuseLo);
	CONSTS->rwLightDirection.Store(coords, OctahedronEncodeNormal(shadingResult.lightDirection));
}
