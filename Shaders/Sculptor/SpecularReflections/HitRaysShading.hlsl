#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS, 2)]]

[[descriptor_set(RTShadingDS, 3)]]
[[descriptor_set(DDGISceneDS, 4)]]
[[descriptor_set(GlobalLightsDS, 5)]]
[[descriptor_set(ShadowMapsDS, 6)]]


#include "Utils/RTVisibilityCommon.hlsli"

#define SPT_LIGHTING_SHADOW_RAY_MISS_SHADER_IDX 0

#include "Utils/SceneViewUtils.hlsli"
#include "Lights/Lighting.hlsli"
#include "SpecularReflections/SRReservoir.hlsli"
#include "SpecularReflections/RTGBuffer.hlsli"


[shader("raygeneration")]
void HitRaysShadingRTG()
{
	const uint2 pixel = DispatchRaysIndex().xy;

	const float depth = u_depthTexture.Load(uint3(pixel, 0));
	if(depth == 0.f)
	{
		return;
	}

	const float2 uv = (pixel + 0.5f) * u_constants.invResolution;
	const float3 ndc = float3(uv * 2.f - 1.f, depth);

	const float3 worldLocation = NDCToWorldSpace(ndc, u_sceneView);

	RTGBuffer<Texture2D> rtgBuffer;
	rtgBuffer.hitNormal            = u_hitNormalTexture;
	rtgBuffer.hitBaseColorMetallic = u_hitBaseColorMetallicTexture;
	rtgBuffer.hitRoughness         = u_hitRoughnessTexture;
	rtgBuffer.rayDistance          = u_rayDistanceTexture;

	const RTGBufferData gBufferData = ReadRTGBuffer(pixel, rtgBuffer);

	const RayHitResult hitResult = UnpackRTGBuffer(gBufferData);

	if(hitResult.hitType == RTGBUFFER_HIT_TYPE_VALID_HIT)
	{
		const float2 encodedRayDirection = u_rayDirectionTexture.Load(uint3(pixel, 0));
		const float3 rayDirection = OctahedronDecodeNormal(encodedRayDirection);

		const float3 hitLocation = worldLocation + rayDirection * hitResult.hitDistance;

		ShadedSurface surface;
		surface.location       = hitLocation;
		surface.shadingNormal  = hitResult.normal;
		surface.geometryNormal = hitResult.normal;
		surface.roughness      = hitResult.roughness;
		ComputeSurfaceColor(hitResult.baseColor, hitResult.metallic, surface.diffuseColor, surface.specularColor);

		const float3 primaryHitToView = normalize(u_sceneView.viewLocation - worldLocation);

		const float3 luminance = CalcReflectedLuminance(surface, -rayDirection, DDGISecondaryBounceSampleContext::Create(worldLocation, primaryHitToView), 1.f);

		const float rayPdf = u_rayPdfTexture.Load(uint3(pixel, 0));

		SRReservoir reservoir = SRReservoir::Create(hitLocation, hitResult.normal, luminance, rayPdf);

		reservoir.luminance = LuminanceToExposedLuminance(reservoir.luminance);

		reservoir.AddFlag(SR_RESERVOIR_FLAGS_RECENT);

		const uint reservoirIdx = GetScreenReservoirIdx(pixel, u_constants.reservoirsResolution);
		u_reservoirsBuffer[reservoirIdx] = PackReservoir(reservoir);
	}
}


[shader("miss")]
void ShadowRayRTM(inout ShadowRayPayload payload)
{
	payload.isShadowed = false;
}
