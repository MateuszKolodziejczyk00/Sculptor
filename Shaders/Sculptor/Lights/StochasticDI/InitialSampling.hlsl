#include "SculptorShader.hlsli"

[[shader_params(RenderSceneConstants, SCENE)]]
[[shader_params(GPURenderView, VIEW)]]

[[shader_params(StochasticDIInitialSamplingConstants, CONSTS)]]

#include "RayTracing/RayTracingHelpers.hlsli"
#include "Utils/GBuffer/GBuffer.hlsli"
#include "Utils/Random.hlsli"
#include "Lights/StochasticDI/StochasticDI.hlsli"
#if !ENABLE_PRESAMPLING
#include "Lights/StochasticDI/EmissiveSampling.hlsli"
#endif // !ENABLE_PRESAMPLING


EmissiveSample UnpackEmissiveSample(in PackedEmissiveSample packed)
{
	EmissiveSample sample;
	sample.location  = packed.location;
	sample.normal    = OctahedronDecodeNormal(packed.normal);
	sample.luminance = DecodeRGBFromLogLuv(packed.packedLuminance);
	sample.pdf_A     = packed.pdf_A;

	return sample;
}


[shader("raygeneration")]
void InitialSamplingRTG()
{
	const uint2 coords = DispatchRaysIndex().xy;

	const GBufferInterface gBuffer = GBufferInterface(CONSTS->gBuffer);

	const SurfaceInfo surface = gBuffer.GetSurfaceInfo(coords);

	const uint reservoirIdx = GetScreenReservoirIdx(coords, CONSTS->reservoirsResolution);

	// Emissive lighting of these pixels is handled by RT GI
	if (surface.roughness < RTClosure::RoughnessNoResamplingThreshold)
	{
		const DIPackedReservoir emptyReservoir = PackDIReservoir(DIReservoir::Create());
		CONSTS->outInitialReservoirs.Store(reservoirIdx, emptyReservoir);
		CONSTS->outReservoirs.Store(reservoirIdx, emptyReservoir);
		return;
	}

	const float3 V = normalize(VIEW->sceneView.viewLocation - surface.location);

	RngState rngState = RngState::Create(coords, SCENE->gpuScene.frameIdx);

	DIReservoir reservoir = DIReservoir::Create();

#if ENABLE_PRESAMPLING
	const uint tileOffset = WaveReadLaneFirst(uint(rngState.Next() * CONSTS->presampledTilesNum)) * CONSTS->presampledTileSize;
#endif // ENABLE_PRESAMPLING

	for (uint i = 0u; i < CONSTS->risSamplesNum; ++i)
	{
#if ENABLE_PRESAMPLING
		const EmissiveSample sample = UnpackEmissiveSample(CONSTS->presampledEmissives.Load(tileOffset + uint(rngState.Next() * CONSTS->presampledTileSize)));
#else
		const EmissiveSample sample = UnpackEmissiveSample(GenerateEmissiveSample(rngState, CONSTS->emissivesSampler));
#endif // ENABLE_PRESAMPLING

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

	{
		DIReservoir initialReservoir = reservoir;
		initialReservoir.Normalize();
		initialReservoir.M = 1u;
		CONSTS->outInitialReservoirs.Store(reservoirIdx, PackDIReservoir(initialReservoir));
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

	CONSTS->outReservoirs.Store(reservoirIdx, PackDIReservoir(reservoir));
}
