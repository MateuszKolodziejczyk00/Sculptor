#include "SculptorShader.hlsli"

[[descriptor_set(ApplyVolumetricFogDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"
#include "Utils/SceneViewUtils.hlsli"

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

		const float depth = u_depthTexture.SampleLevel(u_depthSampler, uv, 0);

		float3 fogFroxelUVW = 0.f;
		if(depth > 0.000001f)
		{
			const float linearDepth = ComputeLinearDepth(depth, u_sceneView);
			fogFroxelUVW = ComputeFogFroxelUVW(uv, linearDepth, u_applyVolumetricFogParams.fogNearPlane, u_applyVolumetricFogParams.fogFarPlane);

			// We use froxel that is closer to avoid light leaking
			const float froxelDepth = 1.f / u_applyVolumetricFogParams.fogResolution.z;
			const float zBias = 1.5f * froxelDepth;
			fogFroxelUVW.z -= zBias;
		}
		else
		{
			fogFroxelUVW = float3(uv, 1.f);
		}

		float4 inScatteringTransmittance = u_integratedInScatteringTexture.SampleLevel(u_integratedInScatteringSampler, fogFroxelUVW, 0);

		float weight = 1.f;

		[unroll]
		for (int i = -1; i <= 1; ++i)
		{
			[unroll]
			for (int j = -1; j <= 1; ++j)
			{
				const float2 currentSampleUV = uv + float2(i, j) * pixelSize * u_applyVolumetricFogParams.blendPixelsOffset;

				inScatteringTransmittance += u_integratedInScatteringTexture.SampleLevel(u_integratedInScatteringSampler, float3(currentSampleUV, fogFroxelUVW.z), 0);
				weight += 1.f;
			}
		}
		 
		inScatteringTransmittance /= weight;

		const float3 inScattering = inScatteringTransmittance.rgb;
		const float transmittance = inScatteringTransmittance.a;

		float3 luminance = ExposedLuminanceToLuminance(u_luminanceTexture[pixel]);
		luminance = luminance * transmittance;
		luminance = LuminanceToExposedLuminance(luminance);
		luminance += inScattering;

		u_luminanceTexture[pixel] = luminance;
	}
}
