#include "SculptorShader.hlsli"

[[shader_params(GPURenderView, VIEW)]]
[[shader_params(CloudscapeConstants, PARAMS_CLOUDSCAPE)]]

[[shader_params(RenderCirrusCloudsConstants, PARAMS_RENDER_CIRRUS_CLOUDS_CONSTANTS)]]

#include "Atmosphere/VolumetricClouds/CloudscapeRaymarcher.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void RenderCirrusCloudsCS(CS_INPUT input)
{
    const uint3 coords = uint3(input.globalID.xy, 0u);

	const float2 uv = (coords.xy + 0.5f) * PARAMS_RENDER_CIRRUS_CLOUDS_CONSTANTS->rcpResolution;

    const float depth = PARAMS_RENDER_CIRRUS_CLOUDS_CONSTANTS->furthestDepth.SampleLevel(BindlessSamplers::LinearMinClampEdge(), uv, 0.f);
	if (depth > 0.f)
	{
		return;
	}

	const float noise = frac(PARAMS_RENDER_CIRRUS_CLOUDS_CONSTANTS->blueNoise.Load(coords & 255u) + ((PARAMS_RENDER_CIRRUS_CLOUDS_CONSTANTS->frameIdx) & 255u) * SPT_GOLDEN_RATIO);

    const Ray viewRay = CreateViewRayWSNoJitter(VIEW->sceneView, uv);

    const float3 skyAvgLuminance = PARAMS_RENDER_CIRRUS_CLOUDS_CONSTANTS->skyProbe.Load(uint3(0u, 0u, 0u));

    const float4 cirrusClouds = SampleCirrusClouds(viewRay, noise,skyAvgLuminance);

    PARAMS_RENDER_CIRRUS_CLOUDS_CONSTANTS->rwCirrusClouds.Store(coords.xy, float4(LuminanceToExposedLuminance(cirrusClouds.rgb), cirrusClouds.a));
}
