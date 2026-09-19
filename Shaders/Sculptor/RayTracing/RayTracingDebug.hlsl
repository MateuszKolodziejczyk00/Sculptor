#include "SculptorShader.hlsli"

[[shader_params(RenderSceneConstants, SCENE)]]
[[shader_params(GPURenderView, VIEW)]]

[[shader_params(RayTracingDebugConstants, PARAMS_RAY_TRACING_DEBUG_CONSTANTS)]]

#include "RayTracing/RayTracingMaterials.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Hashing.hlsli"


struct DebugRayPayloadData
{
	bool isMiss;

	float3 baseColor;

	GPUPtr<RTInstanceInterface> hitInstance;
};


[shader("closesthit")]
void RayTracingDebugCHS(inout DebugRayPayloadData payload, in BuiltInTriangleIntersectionAttributes attrib)
{
	const RTMaterialEvaluationParams evalParams = RTMaterialEvaluationParams::CreateFromAttribs(attrib);

	MaterialEvaluationOutput evaluatedMaterial = RTMaterial::EvaluateMat(evalParams);

	payload.isMiss = false;
	payload.baseColor = evaluatedMaterial.baseColor;
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

	const float2 uv = (float2(coords) + 0.5f) * PARAMS_RAY_TRACING_DEBUG_CONSTANTS->rcpResolution;

	RTSceneInterface rtScene = RTScene();

	const uint instanceMask = RT_INSTANCE_FLAG_OPAQUE;

	DebugRayPayloadData payload;

	RayDesc rayDesc;
	rayDesc.TMin      = 0.f;
	rayDesc.TMax      = 10000.f;
	rayDesc.Origin    = VIEW->sceneView.viewLocation;
	rayDesc.Direction = ComputeViewRayDirectionWS(VIEW->sceneView, uv);

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
		color = float4(payload.baseColor, 1.f);

		if (all(coords == PARAMS_RAY_TRACING_DEBUG_CONSTANTS->debugCrosshairPos))
		{
			RTDebugInstanceInfo debugInstanceInfo;
			debugInstanceInfo.instance = payload.hitInstance.Load();
			PARAMS_RAY_TRACING_DEBUG_CONSTANTS->rwDebugInstanceInfo.Store(0u, debugInstanceInfo);
		}
	}

	// Draw crosshair
	if (any(abs(int2(coords) - int2(PARAMS_RAY_TRACING_DEBUG_CONSTANTS->debugCrosshairPos)) < 2) && all(abs(int2(coords) - int2(PARAMS_RAY_TRACING_DEBUG_CONSTANTS->debugCrosshairPos)) <= 10))
	{
		color = float4(1.f, 0.f, 0.f, 1.f);
	}

	PARAMS_RAY_TRACING_DEBUG_CONSTANTS->rwDebugColor.Store(coords, color);
}
