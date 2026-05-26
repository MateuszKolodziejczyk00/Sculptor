#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS)]]
[[shader_params(TerrainInfluenceGizmoConstants, u_constants)]]

#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


float4 BlendOver(float4 src, float4 dst)
{
	return float4(src.rgb * src.a + dst.rgb * (1.f - src.a), (1.f - (1.f - src.a) * (1.f - dst.a)));
}


[numthreads(16, 16, 1)]
void TerrainInfluenceGizmoCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;
	if (any(pixel >= u_constants.resolution))
	{
		return;
	}

	const float2 mouseUV = u_constants.mouseUV;
	if (any(saturate(mouseUV) != mouseUV))
	{
		return;
	}

	const uint2 mousePixel = min(uint2(mouseUV * u_constants.resolution), u_constants.resolution - 1u);
	const float mouseDepth = u_constants.depth.Load(uint3(mousePixel, 0u));

	float4 output = u_constants.rwOutput.Load(pixel);

	if (mouseDepth > 0.f)
	{
		const Sphere influenceSphere = Sphere::Create(NDCToWorldSpace(float3(mouseUV * 2.f - 1.f, mouseDepth), u_sceneView), u_constants.radius);

		const float sceneDepth = max(u_constants.depth.Load(uint3(pixel, 0u)), 0.00001f);

		const float2 uv = (float2(pixel) + 0.5f) / u_constants.resolution;
		const float3 sceneLocationWS = NDCToWorldSpace(float3(uv * 2.f - 1.f, sceneDepth), u_sceneView);
		const Ray viewRay = CreateViewRayWS(u_sceneView, uv);
		const IntersectionResult sphereIntersection = viewRay.IntersectSphere(influenceSphere);

		const float sceneDist = length(sceneLocationWS - u_sceneView.viewLocation);

		if (sphereIntersection.IsValid() && sphereIntersection.GetTime() < sceneDist)
		{
			output = BlendOver(float4(u_constants.color, u_constants.opacity), output);

			if (!influenceSphere.IsInside(sceneLocationWS))
			{
				output = BlendOver(float4(u_constants.color, u_constants.opacity), output);
			}
		}
	}

	u_constants.rwOutput.Store(pixel, output);
}
