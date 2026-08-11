#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS)]]
[[descriptor_set(CloudscapeDS)]]
[[shader_params(CloudscapeInfluenceGizmoConstants, u_constants)]]

#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(16, 16, 1)]
void CloudscapeInfluenceGizmoCS(CS_INPUT input)
{
	const uint2 coords = input.globalID.xy;
	if (any(coords >= u_constants.resolution))
	{
		return;
	}

	const float2 mouseUV = u_constants.mouseUV;

	const float2 uv = (coords + 0.5f) / u_constants.resolution;
	const Ray viewRay = CreateViewRayWS(u_sceneView, uv);

	const Ray mouseRay = CreateViewRayWS(u_sceneView, mouseUV);

	CloudscapeConstants cloudscape = u_cloudscapeConstants;

	const Sphere atmosphereSphere = Sphere::Create(float3(0.f, 0.f, cloudscape.cloudsAtmosphereCenterZ), cloudscape.cloudsAtmosphereInnerRadius);

	const IntersectionResult innerIntersection = viewRay.IntersectSphere(atmosphereSphere);

	if (innerIntersection.IsValid())
	{
		const float sceneDepth = max(u_constants.depth.Load(uint3(coords, 0u)), 0.00001f);

		const float3 intersectionLocation = viewRay.origin + viewRay.direction * innerIntersection.GetTime();

		const float3 intersectionLocationNDC = WorldSpaceToNDC(intersectionLocation, u_sceneView);
		if (intersectionLocationNDC.z < sceneDepth)
		{
			return;
		}

		const float2 weatherMapUV = intersectionLocation.xy * u_cloudscapeConstants.weatherMapScale + float2(0.5f, 0.5f);
		float4 color = u_weatherMap.SampleLevel(BindlessSamplers::LinearRepeat(), weatherMapUV, 0.f);

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

				const float influenceRadius = u_constants.radius;
				const float dist = distance(intersectionLocation.xy, mouseIntersectionLocation.xy);

				if (dist < influenceRadius && dist > influenceRadius * 0.98f)
				{
					color.xyz = u_constants.color;
				}
			}
		}

		u_constants.rwOutput.Store(coords, color);
	}
}
