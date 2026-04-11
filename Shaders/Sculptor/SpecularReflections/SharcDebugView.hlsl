#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS)]]
[[descriptor_set(SharcCacheDS)]]

[[shader_params(SharcDebugViewConstants, u_constants)]]


#include "SpecularReflections/SculptorSharcQuery.hlsli"
#include "Utils/GBuffer/GBuffer.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(16, 16, 1)]
void SharcDebugViewCS(CS_INPUT input)
{
	const uint2 coords = input.globalID.xy;

	float3 luminance = 0.f;

	const GBufferInterface gbuffer = GBufferInterface(u_constants.gpuGBuffer);

	const SurfaceInfo surface = gbuffer.GetSurfaceInfo(coords);

	if (surface.depth != 0.f)
	{
		SharcQuery query;
		query.location = surface.location;
		query.normal = surface.normal;
#if SHARC_MATERIAL_DEMODULATION
		query.materialDemodulation = 1.f;
#endif // SHARC_MATERIAL_DEMODULATION
		if (!QueryCachedLuminance(u_sceneView.viewLocation, u_viewExposure.exposure, query, OUT luminance))
		{
			luminance = 0.f;

		}
	}

	debug::WriteDebugPixelOnScreen(coords, float4(luminance * u_viewExposure.exposure, 1.f));
}
