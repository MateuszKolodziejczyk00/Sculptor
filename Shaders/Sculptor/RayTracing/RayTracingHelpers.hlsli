#ifndef RAY_TRACING_HELPERS_HLSLI
#define RAY_TRACING_HELPERS_HLSLI

#ifdef DS_RenderSceneDS
#include "RayTracing/RayTracingPayload.hlsli"
#include "RayTracing/RayTracingMaterials.hlsli"

#ifdef RT_MATERIAL_TRACING

[shader("closesthit")]
void GenericCHS(inout RayPayloadData payload, in BuiltInTriangleIntersectionAttributes attrib)
{
	const RTMaterialEvaluationParams evalParams = RTMaterialEvaluationParams::CreateFromAttribs(attrib);

	payload.visibility.isValidHit = evalParams.IsValidHit();

	payload.material.distance = RayTCurrent();

	if (payload.visibility.isValidHit)
	{
		const MaterialEvaluationOutput evaluatedMaterial = RTMaterial::EvaluateMat(evalParams);

		payload.material.Encode(evaluatedMaterial);
	}
}

#endif // RT_MATERIAL_TRACING


[shader("anyhit")]
void GenericAH(inout RayPayloadData payload, in BuiltInTriangleIntersectionAttributes attrib)
{
	const RTMaterialEvaluationParams evalParams = RTMaterialEvaluationParams::CreateFromAttribs(attrib);

	const CustomOpacityOutput opacityOutput = RTMaterial::EvaluateOpacity(evalParams);

	if(opacityOutput.shouldDiscard)
	{
		IgnoreHit();
	}
}


[shader("miss")]
void GenericRTM(inout RayPayloadData payload)
{
	payload.visibility.isMiss = true;
}

#endif // DS_RenderSceneDS
#endif // RAY_TRACING_HELPERS_HLSLI