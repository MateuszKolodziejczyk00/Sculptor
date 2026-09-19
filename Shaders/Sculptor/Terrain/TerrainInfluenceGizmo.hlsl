#include "SculptorShader.hlsli"

[[shader_params(GPURenderView, VIEW)]]
[[shader_params(TerrainInfluenceGizmoConstants, PARAMS_TERRAIN_INFLUENCE_GIZMO_CONSTANTS)]]

#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(16, 16, 1)]
void TerrainInfluenceGizmoCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;
	if (any(pixel >= PARAMS_TERRAIN_INFLUENCE_GIZMO_CONSTANTS->resolution))
	{
		return;
	}

	const float2 mouseUV = PARAMS_TERRAIN_INFLUENCE_GIZMO_CONSTANTS->mouseUV;
	if (all(saturate(mouseUV) != mouseUV))
	{
		return;
	}

	const uint2 mousePixel = min(uint2(mouseUV * PARAMS_TERRAIN_INFLUENCE_GIZMO_CONSTANTS->resolution), PARAMS_TERRAIN_INFLUENCE_GIZMO_CONSTANTS->resolution - 1u);
	const float mouseDepth = PARAMS_TERRAIN_INFLUENCE_GIZMO_CONSTANTS->depth.Load(uint3(mousePixel, 0u));

	float4 output = PARAMS_TERRAIN_INFLUENCE_GIZMO_CONSTANTS->rwOutput.Load(pixel);

	if (mouseDepth > 0.f)
	{
		const Sphere influenceSphere = Sphere::Create(NDCToWorldSpace(float3(mouseUV * 2.f - 1.f, mouseDepth), VIEW->sceneView), PARAMS_TERRAIN_INFLUENCE_GIZMO_CONSTANTS->radius);

		const float sceneDepth = max(PARAMS_TERRAIN_INFLUENCE_GIZMO_CONSTANTS->depth.Load(uint3(pixel, 0u)), 0.00001f);

		const float2 uv = (float2(pixel) + 0.5f) / PARAMS_TERRAIN_INFLUENCE_GIZMO_CONSTANTS->resolution;
		const float3 sceneLocationWS = NDCToWorldSpace(float3(uv * 2.f - 1.f, sceneDepth), VIEW->sceneView);
		const Ray viewRay = CreateViewRayWS(VIEW->sceneView, uv);
		const IntersectionResult sphereIntersection = viewRay.IntersectSphere(influenceSphere);

		const float sceneDist = length(sceneLocationWS - VIEW->sceneView.viewLocation);

		if (sphereIntersection.IsValid() && sphereIntersection.GetTime() < sceneDist)
		{
			output = BlendOver(float4(PARAMS_TERRAIN_INFLUENCE_GIZMO_CONSTANTS->color, PARAMS_TERRAIN_INFLUENCE_GIZMO_CONSTANTS->opacity), output);

			if (!influenceSphere.IsInside(sceneLocationWS))
			{
				output = BlendOver(float4(PARAMS_TERRAIN_INFLUENCE_GIZMO_CONSTANTS->color, PARAMS_TERRAIN_INFLUENCE_GIZMO_CONSTANTS->opacity), output);
			}
		}
	}

	PARAMS_TERRAIN_INFLUENCE_GIZMO_CONSTANTS->rwOutput.Store(pixel, output);
}
