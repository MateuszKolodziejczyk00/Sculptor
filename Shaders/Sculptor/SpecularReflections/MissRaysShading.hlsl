#include "SculptorShader.hlsli"

[[descriptor_set(RTShadingDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "SpecularReflections/SRReservoir.hlsli"
#include "SpecularReflections/RTGBuffer.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 4, 1)]
void MissRaysShadingCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;
	if(all(pixel < u_constants.resolution))
	{
		const float depth = u_depthTexture.Load(uint3(pixel, 0));
		if(depth > 0.f)
		{
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

			if(hitResult.hitType == RTGBUFFER_HIT_TYPE_NO_HIT)
			{
				const float2 encodedRayDirection = u_rayDirectionTexture.Load(uint3(pixel, 0));
				const float3 rayDirection = OctahedronDecodeNormal(encodedRayDirection);

				const float3 locationInAtmoshpere = GetLocationInAtmosphere(u_atmosphereParams, worldLocation);
				const float3 luminance = GetLuminanceFromSkyViewLUT(u_atmosphereParams, u_skyViewLUT, u_linearSampler, locationInAtmoshpere, rayDirection);

				const float rayPdf = u_rayPdfTexture.Load(uint3(pixel, 0));

				const float3 reservoirHitLocation = worldLocation + rayDirection * 2000.f;
				SRReservoir reservoir = SRReservoir::Create(reservoirHitLocation, 0.f, luminance, rayPdf);
				reservoir.AddFlag(SR_RESERVOIR_FLAGS_MISS);
				reservoir.AddFlag(SR_RESERVOIR_FLAGS_RECENT);

				reservoir.luminance = LuminanceToExposedLuminance(reservoir.luminance);

				const uint reservoirIdx = GetScreenReservoirIdx(pixel, u_constants.reservoirsResolution);
				u_reservoirsBuffer[reservoirIdx] = PackReservoir(reservoir);
			}
		}
	}
}
