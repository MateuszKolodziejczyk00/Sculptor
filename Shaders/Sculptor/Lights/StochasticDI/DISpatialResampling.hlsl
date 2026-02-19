#include "SculptorShader.hlsli"

[[descriptor_set(RenderSceneDS)]]
[[descriptor_set(RenderViewDS)]]

[[shader_params(DISpatialResamplingConstants , u_constants)]]

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

	const GBufferInterface gBuffer = GBufferInterface(u_constants.gBuffer);

	const SurfaceInfo surface = gBuffer.GetSurfaceInfo(coords);

	const float3 V = normalize(u_sceneView.viewLocation - surface.location);

	DIReservoir reservoir = DIReservoir::Create();

	const uint reservoirIdx = GetScreenReservoirIdx(coords, u_constants.reservoirsResolution);
	const DIReservoir initialReservoir = UnpackDIReservoir(u_constants.inReservoirs.Load(reservoirIdx));

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

	RngState rngState = RngState::Create(coords, u_renderSceneConstants.gpuScene.frameIdx + 7u);

	const int range       = u_constants.spatialResamplingRange;
	const uint samplesNum = u_constants.spatialSamplesNum;

	for (uint i = 0u; i < samplesNum; ++i)
	{
#if WAVE_COHERENT_SPATIAL_RESAMPLING
		const int2 offset = WaveReadLaneFirst((float2(rngState.Next(), rngState.Next()) * 2.f - 1.f) * range);
#else
		const int2 offset = (float2(rngState.Next(), rngState.Next()) * 2.f - 1.f) * range;
#endif

		int2 sampleCoords = abs(int2(coords) + offset);
		if (sampleCoords.x >= u_constants.gBuffer.resolution.x)
		{
			sampleCoords.x = 2 * (u_constants.gBuffer.resolution.x - 1) - sampleCoords.x;
		}
		if (sampleCoords.y >= u_constants.gBuffer.resolution.y)
		{
			sampleCoords.y = 2 * (u_constants.gBuffer.resolution.y - 1) - sampleCoords.y;
		}

#if ENABLE_SURFACE_CHECK
		const SurfaceInfo sampleSurface = gBuffer.GetSurfaceInfo(sampleCoords);
		if (!CanResampleSurface(surface, sampleSurface.location, sampleSurface.normal))
		{
			continue;
		}
#endif

		const uint sampleReservoirIdx = GetScreenReservoirIdx(sampleCoords, u_constants.reservoirsResolution);
		const DIReservoir sampleReservoir = UnpackDIReservoir(u_constants.inReservoirs.Load(sampleReservoirIdx));

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
		const float3 toSample = reservoir.sample.location - surface.location;

		RayDesc rayDesc;
		rayDesc.TMin      = 0.01f;
		rayDesc.TMax      = length(toSample) - 0.02f;
		rayDesc.Origin    = surface.location + surface.normal * 0.01f;
		rayDesc.Direction = normalize(toSample);

		if (!RTScene().VisibilityTest(rayDesc))
		{
			reservoir.weightSum = 0.f;
		}
	}

	u_constants.outReservoirs.Store(reservoirIdx, PackDIReservoir(reservoir));

	const StochasticDIShadingResult shadingResult = FinalDIShading(surface, V, reservoir.sample, reservoir.weightSum);

	float3 diffuseLo = shadingResult.diffuse;
	diffuseLo = LuminanceToExposedLuminance(diffuseLo);
	diffuseLo /= max(surface.diffuseColor, SMALL_NUMBER);

	const float NdotV = saturate(dot(surface.normal, V));
	const float2 integratedBRDF = u_constants.brdfIntegrationLUT.SampleLevel(BindlessSamplers::LinearClampEdge(), float2(NdotV, surface.roughness), 0);

	float3 specularLo = shadingResult.specular;
	specularLo = LuminanceToExposedLuminance(specularLo);
	specularLo /= max((surface.specularColor * integratedBRDF.x + integratedBRDF.y), 0.01f);

	u_constants.rwSpecularHitDist.Store(coords, float4(specularLo, shadingResult.hitDistance));
	u_constants.rwDiffuse.Store(coords, diffuseLo);
	u_constants.rwLightDirection.Store(coords, OctahedronEncodeNormal(shadingResult.lightDirection));
}
