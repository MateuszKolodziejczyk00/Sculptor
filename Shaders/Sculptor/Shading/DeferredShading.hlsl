#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS)]]
[[descriptor_set(RenderSceneDS)]]

[[descriptor_set(DeferredShadingDS)]]

[[descriptor_set(ViewShadingInputDS)]]

#if ENABLE_DDGI

[[descriptor_set(DDGISceneDS, 5)]]

#include "DDGI/DDGITypes.hlsli"

#endif // ENABLE_DDGI

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/GBuffer/GBuffer.hlsli"
#include "Lights/Lighting.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


GBufferData LoadGBuffer(uint3 pixel)
{
	GBufferInput gbufferInput;
	gbufferInput.gBuffer0 = u_gBuffer0Texture;
	gbufferInput.gBuffer1 = u_gBuffer1Texture;
	gbufferInput.gBuffer2 = u_gBuffer2Texture;
	gbufferInput.gBuffer3 = u_gBuffer3Texture;
	gbufferInput.gBuffer4 = u_gBuffer4Texture;

	return DecodeGBuffer(gbufferInput, pixel);
}


[numthreads(8, 8, 1)]
void DeferredShadingCS(CS_INPUT input)
{
	const uint3 pixel = input.globalID;

	float3 luminance = 0.f;

	if(all(pixel.xy < u_deferredShadingConstants.resolution))
	{
		const float depth = u_depthTexture.Load(pixel).x;

		if(depth > 0.f)
		{
			const float2 screenUV = (float2(pixel.xy) + 0.5f) * u_deferredShadingConstants.pixelSize;
			const float3 ndc = float3(screenUV * 2.f - 1.f, depth);

			const float3 worldLocation = NDCToWorldSpace(ndc, u_sceneView);

			const GBufferData gBufferData = LoadGBuffer(pixel);

			ShadedSurface surface;
			surface.location       = worldLocation;
			surface.shadingNormal  = gBufferData.normal;
			surface.geometryNormal = gBufferData.normal;
			surface.roughness      = gBufferData.roughness;
			surface.uv             = screenUV;
			surface.linearDepth    = ComputeLinearDepth(depth, u_sceneView);
			
			ComputeSurfaceColor(gBufferData.baseColor, gBufferData.metallic, OUT surface.diffuseColor, OUT surface.specularColor);

			const float3 toView = normalize(u_sceneView.viewLocation - worldLocation);

			ViewLightingAccumulator lightingAccumulator = ViewLightingAccumulator::Create();
			CalcReflectedLuminance(surface, toView, INOUT lightingAccumulator);


#if TILED_SHADING_DEBUG
			TiledShadingDebug(pixel.xy, surface);
#endif // TILED_SHADING_DEBUG

			float3 indirectIlluminance = 0.f;

#if ENABLE_DDGI
			float ambientOcclusion = 1.f;

			if (u_deferredShadingConstants.isAmbientOcclusionEnabled)
			{
				ambientOcclusion = u_ambientOcclusionTexture.Load(pixel);
			}

			DDGISampleParams ddgiSampleParams = CreateDDGISampleParams(worldLocation, surface.geometryNormal, toView);
			ddgiSampleParams.sampleDirection = surface.shadingNormal;

			const float random = u_blueNoise256Texture.Load(uint3(pixel.xy & 255, 0)).x;

			// Use blue noise to blend between the two volumes. This way we can sample only one volume per pixel.
			indirectIlluminance = DDGISampleIlluminanceBlended(ddgiSampleParams, random, DDGISampleContext::Create()) * ambientOcclusion;
#endif // ENABLE_DDGI

			const float3 indirectDiffuse = Diffuse_Lambert(indirectIlluminance);
			lightingAccumulator.Accumulate(LightingContribution::Create(surface.diffuseColor * indirectDiffuse, indirectDiffuse));

			lightingAccumulator.Accumulate(LightingContribution::Create(gBufferData.emissive));

			luminance = LuminanceToExposedLuminance(lightingAccumulator.GetLuminance());
		}
	}

	u_luminanceTexture[pixel.xy] = float4(luminance, 1.f);
}
