#include "SculptorShader.hlsli"

[[descriptor_set(RenderSceneDS)]]
[[descriptor_set(RenderViewDS)]]
[[shader_params(TerrainMaterialPaintCommandConstants, u_constants)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Terrain/SceneTerrain.hlsli" 


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


static const float2 kMaterialMapMinBounds = -1024.f;
static const float2 kMaterialMapSpan      = 2048.f;


[numthreads(16, 16, 1)]
void TerrainMaterialPaintCommandCS(CS_INPUT input)
{
	const uint2 texel = input.globalID.xy;
	const uint2 materialMapResolution = u_constants.rwMaterialIDs.GetResolution();
	if (any(texel >= materialMapResolution))
	{
		return;
	}

	const float2 mouseUV = u_constants.mouseUV;
	if (any(saturate(mouseUV) != mouseUV))
	{
		return;
	}

	const uint2 screenResolution = u_constants.depth.GetResolution();
	const uint2 mousePixel = min(uint2(mouseUV * screenResolution), screenResolution - 1u);
	const float mouseDepth = u_constants.depth.Load(uint3(mousePixel, 0u));
	if (mouseDepth <= 0.f)
	{
		return;
	}

	const TerrainInterface terrain = SceneTerrain();

	const float3 mouseWorldLocation = NDCToWorldSpace(float3(mouseUV * 2.f - 1.f, mouseDepth), u_sceneView);

	const float2 texelUV = (float2(texel) + 0.5f) / materialMapResolution;

	const float2 texelWorldLocation = terrain.materialsMap.minBounds + texelUV / terrain.materialsMap.rcpBoundsSize;

	const float2 offsetFromBrushCenter = texelWorldLocation - mouseWorldLocation.xy;
	if (dot(offsetFromBrushCenter, offsetFromBrushCenter) <= u_constants.radius * u_constants.radius)
	{
		u_constants.rwMaterialIDs.Store(texel, u_constants.materialID);
	}
}
