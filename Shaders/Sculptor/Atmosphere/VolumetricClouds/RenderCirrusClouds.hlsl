#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS)]]
[[descriptor_set(CloudscapeDS)]]

[[shader_params(RenderCirrusCloudsConstants, u_constants)]]

#include "Atmosphere/VolumetricClouds/CloudscapeRaymarcher.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void RenderCirrusCloudsCS(CS_INPUT input)
{
    const uint3 coords = uint3(input.globalID.xy, 0u);

	const float2 uv = (coords.xy + 0.5f) * u_constants.rcpResolution;

    const float depth = u_constants.furthestDepth.SampleLevel(BindlessSamplers::LinearMinClampEdge(), uv, 0.f);
	if (depth > 0.f)
	{
		return;
	}

	const float noise = frac(u_constants.blueNoise.Load(coords & 255u) + ((u_constants.frameIdx) & 255u) * SPT_GOLDEN_RATIO);

    const Ray viewRay = CreateViewRayWSNoJitter(u_sceneView, uv);

    const float3 skyAvgLuminance = u_constants.skyProbe.Load(uint3(0u, 0u, 0u));

    const float4 cirrusClouds = SampleCirrusClouds(viewRay, noise,skyAvgLuminance);

    u_constants.rwCirrusClouds.Store(coords.xy, float4(LuminanceToExposedLuminance(cirrusClouds.rgb), cirrusClouds.a));
}
