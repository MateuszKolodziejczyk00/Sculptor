#include "SculptorShader.hlsli"

[[shader_params(GPURenderView, VIEW)]]
[[shader_params(CloudscapeConstants, PARAMS_CLOUDSCAPE)]]
[[shader_params(CloudscapeInfluenceGizmoConstants, PARAMS_CLOUDSCAPE_INFLUENCE_GIZMO_CONSTANTS)]]

#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(16, 16, 1)]
void CloudscapeInfluenceGizmoCS(CS_INPUT input)
{
	const uint2 coords = input.globalID.xy;
	if (any(coords >= PARAMS_CLOUDSCAPE_INFLUENCE_GIZMO_CONSTANTS->resolution))
	{
		return;
	}

	const float2 mouseUV = PARAMS_CLOUDSCAPE_INFLUENCE_GIZMO_CONSTANTS->mouseUV;

	const float2 uv = (coords + 0.5f) / PARAMS_CLOUDSCAPE_INFLUENCE_GIZMO_CONSTANTS->resolution;
	const Ray viewRay = CreateViewRayWS(VIEW->sceneView, uv);

	const Ray mouseRay = CreateViewRayWS(VIEW->sceneView, mouseUV);

	CloudscapeConstants cloudscape = *PARAMS_CLOUDSCAPE;

	const Sphere atmosphereSphere = Sphere::Create(float3(0.f, 0.f, cloudscape.cloudsAtmosphereCenterZ), cloudscape.cloudsAtmosphereInnerRadius);

	const IntersectionResult innerIntersection = viewRay.IntersectSphere(atmosphereSphere);

	if (innerIntersection.IsValid())
	{
		const float sceneDepth = max(PARAMS_CLOUDSCAPE_INFLUENCE_GIZMO_CONSTANTS->depth.Load(uint3(coords, 0u)), 0.00001f);

		const float3 intersectionLocation = viewRay.origin + viewRay.direction * innerIntersection.GetTime();

		const float3 intersectionLocationNDC = WorldSpaceToNDC(intersectionLocation, VIEW->sceneView);
		if (intersectionLocationNDC.z < sceneDepth)
		{
			return;
		}

		const float2 weatherMapUV = intersectionLocation.xy * PARAMS_CLOUDSCAPE->weatherMapScale + float2(0.5f, 0.5f);
		float4 color = PARAMS_CLOUDSCAPE->weatherMap.SampleLevel(BindlessSamplers::LinearRepeat(), weatherMapUV, 0.f);

		float2 grid = frac(intersectionLocation.xy * 0.004f);
		grid = min(grid, 1.f - grid);
		float gridAlpha = saturate(1.f - min(grid.x, grid.y) * 50.f);
		if (gridAlpha > 0.f)
		{
			color = float4(1.f, 1.f, 1.f, gridAlpha);
		}

		if (all(saturate(mouseUV) == mouseUV))
		{
			const IntersectionResult mouseIntersection = mouseRay.IntersectSphere(atmosphereSphere);
			if (mouseIntersection.IsValid())
			{
				const float3 mouseIntersectionLocation = mouseRay.origin + mouseRay.direction * mouseIntersection.GetTime();

				const float influenceRadius = PARAMS_CLOUDSCAPE_INFLUENCE_GIZMO_CONSTANTS->radius;
				const float dist = distance(intersectionLocation.xy, mouseIntersectionLocation.xy);

				if (dist < influenceRadius && dist > influenceRadius * 0.98f)
				{
					color.xyz = PARAMS_CLOUDSCAPE_INFLUENCE_GIZMO_CONSTANTS->color;
				}
			}
		}

		PARAMS_CLOUDSCAPE_INFLUENCE_GIZMO_CONSTANTS->rwOutput.Store(coords, color);
	}
}
