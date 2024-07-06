#include "SculptorShader.hlsli"

[[descriptor_set(ApplyVolumetricFogDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Sampling.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void ApplyVolumetricFogCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;

	uint2 outputRes;
	u_luminanceTexture.GetDimensions(outputRes.x, outputRes.y);

	if (all(pixel < outputRes))
	{
		const float2 pixelSize = rcp(float2(outputRes));
		const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

		const float depth = max(u_depthTexture.SampleLevel(u_depthSampler, uv, 0), 0.000001f);

		float3 fogFroxelUVW = 0.f;
		const float linearDepth = ComputeLinearDepth(depth, u_sceneView);
		fogFroxelUVW = ComputeFogFroxelUVW(uv, linearDepth, u_applyVolumetricFogParams.fogNearPlane, u_applyVolumetricFogParams.fogFarPlane);

		// We use froxel that is closer to avoid light leaking
		const float froxelDepth = 1.f / u_applyVolumetricFogParams.fogResolution.z;
		const float zBias = 2 * froxelDepth;
		fogFroxelUVW.z -= zBias;

		const float4 inScatteringTransmittance = SampleTricubic(u_integratedInScatteringTexture, u_integratedInScatteringSampler, fogFroxelUVW, u_applyVolumetricFogParams.fogResolution);

		const float3 inScattering = inScatteringTransmittance.rgb;
		const float transmittance = inScatteringTransmittance.a;

		float3 luminance = ExposedLuminanceToLuminance(u_luminanceTexture[pixel]);
		luminance = luminance * transmittance;
		luminance = LuminanceToExposedLuminance(luminance);
		luminance += inScattering;

		u_luminanceTexture[pixel] = luminance;
	}
}
