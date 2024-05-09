#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(IndirectInScatteringDS, 1)]]
[[descriptor_set(DDGISceneDS, 2)]]

#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"
#include "DDGI/DDGITypes.hlsli"
#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(4, 4, 2)]
void ComputeIndirectInScatteringCS(CS_INPUT input)
{
	uint3 volumetricFogResolution;
	u_inScatteringTexture.GetDimensions(volumetricFogResolution.x, volumetricFogResolution.y, volumetricFogResolution.z);

	if (all(input.globalID < volumetricFogResolution))
	{
		const float3 fogResolutionRcp = rcp(float3(volumetricFogResolution));
		const float3 fogFroxelUVW = (float3(input.globalID) + 0.5f) * fogResolutionRcp;

		const float projectionNearPlane = GetNearPlane(u_sceneView);

		const float fogNearPlane = u_inScatteringParams.fogNearPlane;
		const float fogFarPlane = u_inScatteringParams.fogFarPlane;
		
		const float3 jitter = u_inScatteringParams.jitter;

		const float fogFroxelDepth = fogFroxelUVW.z + jitter.z;
		const float fogFroxelLinearDepth = ComputeFogFroxelLinearDepth(fogFroxelDepth, fogNearPlane, fogFarPlane);

		const float3 fogFroxelNDC = FogFroxelToNDC(fogFroxelUVW.xy + jitter.xy, fogFroxelLinearDepth, projectionNearPlane);

		const float3 fogFroxelWorldLocation = NDCToWorldSpaceNoJitter(fogFroxelNDC, u_sceneView);
		
		DDGISampleParams ddgiSampleParams = CreateDDGISampleParams(fogFroxelWorldLocation, 0.f, 0.f);
		ddgiSampleParams.sampleLocationBiasMultiplier = 0.f;
		ddgiSampleParams.minVisibility                = 0.95f;
		const float3 indirect = DDGISampleAverageLuminance(ddgiSampleParams);
		const float phaseFunction = 1.f / (4.f * PI); // no anisotrophy

		const float3 inScattering = indirect * phaseFunction;

		const float integrationDomain = 4.f * PI;

		u_inScatteringTexture[input.globalID] = inScattering * integrationDomain;
	}
}
