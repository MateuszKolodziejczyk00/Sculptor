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


GBufferData LoadGBuffer(uint3 pixel)
{
	GBufferInput gbufferInput;
	gbufferInput.gBuffer0 = PARAMS_DEFERRED_SHADING->gBuffer0Texture;
	gbufferInput.gBuffer1 = PARAMS_DEFERRED_SHADING->gBuffer1Texture;
	gbufferInput.gBuffer2 = PARAMS_DEFERRED_SHADING->gBuffer2Texture;
	gbufferInput.gBuffer3 = PARAMS_DEFERRED_SHADING->gBuffer3Texture;
	gbufferInput.gBuffer4 = PARAMS_DEFERRED_SHADING->gBuffer4Texture;

	return DecodeGBuffer(gbufferInput, pixel);
}


[numthreads(8, 8, 1)]
void DeferredShadingCS(CS_INPUT input)
{
	const uint3 pixel = input.globalID;

	float3 luminance = 0.f;

	if(all(pixel.xy < PARAMS_DEFERRED_SHADING->resolution))
	{
		const float depth = PARAMS_DEFERRED_SHADING->depthTexture.Load(pixel).x;

		if(depth > 0.f)
		{
			const float2 screenUV = (float2(pixel.xy) + 0.5f) * PARAMS_DEFERRED_SHADING->pixelSize;
			const float3 ndc = float3(screenUV * 2.f - 1.f, depth);

			const float3 worldLocation = NDCToWorldSpace(ndc, VIEW->sceneView);

			const GBufferData gBufferData = LoadGBuffer(pixel);

			ShadedSurface surface;
			surface.location       = worldLocation;
			surface.shadingNormal  = gBufferData.normal;
			surface.geometryNormal = gBufferData.normal;
			surface.roughness      = gBufferData.roughness;
			surface.uv             = screenUV;
			surface.linearDepth    = ComputeLinearDepth(depth, VIEW->sceneView);
			
			ComputeSurfaceColor(gBufferData.baseColor, gBufferData.metallic, OUT surface.diffuseColor, OUT surface.specularColor);

			const float3 toView = normalize(VIEW->sceneView.viewLocation - worldLocation);

			ViewLightingAccumulator lightingAccumulator = ViewLightingAccumulator::Create();
			CalcReflectedLuminance(surface, toView, INOUT lightingAccumulator);


#if TILED_SHADING_DEBUG
			TiledShadingDebug(pixel.xy, surface);
#endif // TILED_SHADING_DEBUG

			float3 indirectIlluminance = 0.f;

#if ENABLE_DDGI
			float ambientOcclusion = 1.f;

			if (PARAMS_DEFERRED_SHADING->isAmbientOcclusionEnabled)
			{
				ambientOcclusion = PARAMS_VIEW_SHADING_INPUT->ambientOcclusionTexture.Load(pixel);
			}

			DDGISampleParams ddgiSampleParams = CreateDDGISampleParams(worldLocation, surface.geometryNormal, toView);
			ddgiSampleParams.sampleDirection = surface.shadingNormal;

			const float random = PARAMS_DEFERRED_SHADING->blueNoise256Texture.Load(uint3(pixel.xy & 255, 0)).x;

			// Use blue noise to blend between the two volumes. This way we can sample only one volume per pixel.
			indirectIlluminance = DDGISampleIlluminanceBlended(ddgiSampleParams, random, DDGISampleContext::Create()) * ambientOcclusion;
#endif // ENABLE_DDGI

			const float3 indirectDiffuse = Diffuse_Lambert(indirectIlluminance);
			lightingAccumulator.Accumulate(LightingContribution::Create(surface.diffuseColor * indirectDiffuse, indirectDiffuse));

			lightingAccumulator.Accumulate(LightingContribution::Create(gBufferData.emissive));

			luminance = LuminanceToExposedLuminance(lightingAccumulator.GetLuminance());
		}
	}

	PARAMS_DEFERRED_SHADING->luminanceTexture[pixel.xy] = float4(luminance, 1.f);
}
