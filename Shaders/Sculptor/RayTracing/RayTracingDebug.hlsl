#include "SculptorShader.hlsli"

[[descriptor_set(RenderSceneDS)]]
[[descriptor_set(RenderViewDS)]]

[[shader_params(RayTracingDebugConstants, u_constants)]]

#include "RayTracing/RayTracingMaterials.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Hashing.hlsli"


struct DebugRayPayloadData
{
	bool isMiss;

	GPUPtr<RTInstanceInterface> hitInstance;
};


[shader("closesthit")]
void RayTracingDebugCHS(inout DebugRayPayloadData payload, in BuiltInTriangleIntersectionAttributes attrib)
{
	const RTMaterialEvaluationParams evalParams = RTMaterialEvaluationParams::CreateFromAttribs(attrib);

	payload.isMiss = false;
	payload.hitInstance = evalParams.hitInstance;
}


[shader("anyhit")]
void RayTracingDebugAH(inout DebugRayPayloadData payload, in BuiltInTriangleIntersectionAttributes attrib)
{
	const RTMaterialEvaluationParams evalParams = RTMaterialEvaluationParams::CreateFromAttribs(attrib);

	const CustomOpacityOutput opacityOutput = RTMaterial::EvaluateOpacity(evalParams);

	if(opacityOutput.shouldDiscard)
	{
		IgnoreHit();
	}
}


[shader("miss")]
void RayTracingDebugRTM(inout DebugRayPayloadData payload)
{
	payload.isMiss = true;
}


[shader("raygeneration")]
void RayTracingDebugRTG()
{
	const uint2 coords = DispatchRaysIndex().xy;

	const float2 uv = (float2(coords) + 0.5f) * u_constants.rcpResolution;

	RTSceneInterface rtScene = RTScene();

	const uint instanceMask = RT_INSTANCE_FLAG_OPAQUE;

	DebugRayPayloadData payload;

	RayDesc rayDesc;
	rayDesc.TMin      = 0.f;
	rayDesc.TMax      = 10000.f;
	rayDesc.Origin    = u_sceneView.viewLocation;
	rayDesc.Direction = ComputeViewRayDirectionWS(u_sceneView, uv);

	TraceRay(rtScene.tlas.GetResource(),
			 0,
			 instanceMask,
			 0,
			 1,
			 0,
			 rayDesc,
			 payload);

	float4 color = 0.f;
	if (!payload.isMiss)
	{
		if (payload.hitInstance.IsValid())
		{
			uint entityIdx = 0u;
			RTInstanceInterface instanceData = payload.hitInstance.Load();
			if (instanceData.entity.IsValid())
			{
				entityIdx = instanceData.entity.GetIndex();
			}

			const uint valueHash = HashPCG(entityIdx);
			color.xyz = float3((valueHash >> 16) & 0xFF, (valueHash >> 8) & 0xFF, valueHash & 0xFF) / 255.f;
			color.w = 1.f;
		}

		if (all(coords == u_constants.debugCrosshairPos))
		{
			RTDebugInstanceInfo debugInstanceInfo;
			debugInstanceInfo.instance = payload.hitInstance.Load();
			u_constants.rwDebugInstanceInfo.Store(0u, debugInstanceInfo);
		}
	}

	// Draw crosshair
	if (any(abs(int2(coords) - int2(u_constants.debugCrosshairPos)) < 2) && all(abs(int2(coords) - int2(u_constants.debugCrosshairPos)) <= 10))
	{
		color = float4(1.f, 0.f, 0.f, 1.f);
	}

	u_constants.rwDebugColor.Store(coords, color);
}
