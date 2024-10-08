#include "SculptorShader.hlsli"

[[descriptor_set(RenderVolumetricFogDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]
[[descriptor_set(IntegrateInScatteringDS, 2)]]

#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"
#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void IntegrateInScatteringCS(CS_INPUT input)
{
	if (all(input.globalID.xy < u_fogConstants.fogGridRes.xy))
	{
		const float2 uv = (input.globalID.xy + 0.5f) * u_fogConstants.fogGridInvRes.xy;

		float currentZ = 0.f;

		float3 integratedScattering = 0.f;
		float integratedTransmittance = 1.f;

		for (uint z = 0; z < u_fogConstants.fogGridRes.z; ++z)
		{
			const float fogFroxelDepth = (z + 0.5f) * u_fogConstants.fogGridInvRes.z;
			const float nextZ = ComputeFogFroxelLinearDepth(fogFroxelDepth, u_fogConstants.fogNearPlane, u_fogConstants.fogFarPlane);
			
			const float4 inScatteringExtinction = u_inScatteringTexture.SampleLevel(u_inScatteringSampler, float3(uv, z * u_fogConstants.fogGridInvRes.z), 0);

			const float3 inScattering = inScatteringExtinction.rgb;
			const float extinction = inScatteringExtinction.a;

			const float clampedExtinction = max(extinction, 0.0001f);

			const float transmittance = exp(-clampedExtinction * (nextZ - currentZ));

			const float3 scattering = (inScattering * (1.f - transmittance)) / clampedExtinction;

			integratedScattering += scattering * integratedTransmittance;
			integratedTransmittance *= max(transmittance, 0.f);

			currentZ = nextZ;
		
			u_integratedInScatteringTexture[uint3(input.globalID.xy, z)] = float4(integratedScattering, integratedTransmittance);
		}
	}
}
