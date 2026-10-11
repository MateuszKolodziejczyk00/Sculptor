#include "SculptorShader.hlsli"

[[shader_params(GPURenderView, VIEW)]]
[[shader_params(RenderSceneConstants, SCENE)]]

[[shader_params(DeferredShadingContstants, PARAMS_DEFERRED_SHADING)]]

[[shader_params(ViewShadingParams, PARAMS_VIEW_SHADING_INPUT)]]

#if ENABLE_DDGI

[[shader_params(DDGIGPUScene, PARAMS_D_D_G_I_SCENE)]]

#include "DDGI/DDGITypes.hlsli"

#endif // ENABLE_DDGI

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/GBuffer/GBuffer.hlsli"
#include "Lights/Lighting.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void DeferredShadingCS(CS_INPUT input)
{
	const uint3 pixel = input.globalID;

	float3 luminance = 0.f;

	if(all(pixel.xy < PARAMS_DEFERRED_SHADING->resolution))
	{
		const SurfaceInfo surfaceInfo = PARAMS_DEFERRED_SHADING->gBuffer.GetSurfaceInfo(pixel.xy);

		if(surfaceInfo.depth > 0.f)
		{
			SurfaceGeometry surfaceGeometry;
			surfaceGeometry.location  = surfaceInfo.location;
			surfaceGeometry.geoNormal = surfaceInfo.normal;

			const ShadedSurface surface = surfaceInfo.GetShadingSurface();

			ClusterInfo cluster;
			cluster.uv          = surfaceInfo.uv;
			cluster.linearDepth = ComputeLinearDepth(surfaceInfo.depth, VIEW->sceneView);

			const float3 toView = normalize(VIEW->sceneView.viewLocation - surfaceInfo.location);

			ViewLightingAccumulator lightingAccumulator = ViewLightingAccumulator::Create();
			CalcReflectedLuminance(surface, surfaceGeometry, cluster, toView, INOUT lightingAccumulator);

#if TILED_SHADING_DEBUG
			TiledShadingDebug(pixel.xy, surface, surfaceGeometry, cluster);
#endif // TILED_SHADING_DEBUG

			float3 indirectIlluminance = 0.f;

#if ENABLE_DDGI
			float ambientOcclusion = 1.f;

			if (PARAMS_DEFERRED_SHADING->isAmbientOcclusionEnabled)
			{
				ambientOcclusion = PARAMS_VIEW_SHADING_INPUT->ambientOcclusionTexture.Load(pixel);
			}

			DDGISampleParams ddgiSampleParams = CreateDDGISampleParams(surfaceInfo.location, surfaceGeometry.geoNormal, toView);
			ddgiSampleParams.sampleDirection = surface.normal;

			const float random = GLOBALS->blueNoise256.Load(uint3(pixel.xy & 255, 0));

			// Use blue noise to blend between the two volumes. This way we can sample only one volume per pixel.
			indirectIlluminance = DDGISampleIlluminanceBlended(ddgiSampleParams, random, DDGISampleContext::Create()) * ambientOcclusion;
#endif // ENABLE_DDGI

			// Albedo is applied when creating lighting contribution
			const float3 indirectDiffuse = brdf_utils::lambert::Evaluate(1.f) * indirectIlluminance;
			lightingAccumulator.Accumulate(LightingContribution::Create(surface.albedo * indirectDiffuse, indirectDiffuse));

			lightingAccumulator.Accumulate(LightingContribution::Create(surfaceInfo.emissive));

			luminance = LuminanceToExposedLuminance(lightingAccumulator.GetLuminance());
		}
	}

	PARAMS_DEFERRED_SHADING->luminanceTexture[pixel.xy] = float4(luminance, 1.f);
}
