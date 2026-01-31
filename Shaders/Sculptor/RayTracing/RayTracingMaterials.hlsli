#ifndef RAY_TRACING_MATERIALS_HLSLI
#define RAY_TRACING_MATERIALS_HLSLI

#include "SceneRendering/GPUScene.hlsli"


#define SPT_MATERIAL_SAMPLE_EXPLICIT_LEVEL 2

#include "Materials/MaterialSystem.hlsli"


struct RTMaterialEvaluationParams
{
	GPUPtr<RTInstanceInterface> hitInstance;

	uint   triangleIdx;
	float2 barycentricCoords;
	bool   isBackface;

	float3 hitLocationWS;

	static RTMaterialEvaluationParams CreateFromAttribs(in BuiltInTriangleIntersectionAttributes attrib)
	{
		RTMaterialEvaluationParams params;
		params.hitInstance       = RTScene().GetInstancePtr(InstanceID());
		params.triangleIdx       = PrimitiveIndex();
		params.barycentricCoords = float2(attrib.barycentrics.x, attrib.barycentrics.y);
		params.isBackface        = (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE);
		params.hitLocationWS     = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
		return params;
	}

	bool IsValidHit()
	{
		bool validHit = true;

		if (isBackface)
		{
			const RTInstanceInterface rtInstance = hitInstance.Load();
			if (!rtInstance.IsDoubleSided())
			{
				validHit = false;
			}
		}
		
		return validHit;
	}
};


namespace RTMaterial
{

#ifdef SPT_MATERIAL_SHADER_PATH

MaterialEvaluationOutput EvaluateMat(in RTMaterialEvaluationParams evalParams)
{
	const RTInstanceInterface instanceData = evalParams.hitInstance.Load();

	const float3 barycentricCoords = float3(1.f - evalParams.barycentricCoords.x - evalParams.barycentricCoords.y, evalParams.barycentricCoords.x, evalParams.barycentricCoords.y);

	uint3 indices;
	[unroll]
	for (uint idx = 0; idx < 3; ++idx)
	{
		indices[idx] = UGB().LoadVertexIndex(instanceData.indicesDataUGBOffset, evalParams.triangleIdx * 3 + idx);
	}

	float3 normal = 0.f;
	[unroll]
	for (uint idx = 0; idx < 3; ++idx)
	{
		normal += UGB().LoadNormal(instanceData.normalsDataUGBOffset, indices[idx]) * barycentricCoords[idx];
	}

	normal = mul(instanceData.entity.Load().transform, float4(normal, 0.f)).xyz;
	normal = normalize(normal);

	if (evalParams.isBackface)
	{
		normal = -normal;
	}

	float2 normalizedUV = 0.f;
	[unroll]
	for (uint idx = 0; idx < 3; ++idx)
	{
		normalizedUV += UGB().LoadNormalizedUV(instanceData.uvsDataUGBOffset, indices[idx]) * barycentricCoords[idx];
	}
	const float2 uv = instanceData.uvsMin + normalizedUV * instanceData.uvsRange;

	const SPT_MATERIAL_DATA_TYPE materialData = LoadMaterialData(instanceData.materialDataHandle);

	MaterialEvaluationParameters materialEvalParams;
	materialEvalParams.clipSpace = -1.f;
	materialEvalParams.normal = normal;
	materialEvalParams.hasTangent = false;
	materialEvalParams.uv = uv;
	materialEvalParams.worldLocation = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();

	const MaterialEvaluationOutput evaluatedMaterial = EvaluateMaterial(materialEvalParams, materialData);

	return evaluatedMaterial;
}


CustomOpacityOutput EvaluateOpacity(in RTMaterialEvaluationParams evalParams)
{
	const RTInstanceInterface instanceData = evalParams.hitInstance.Load();

	const float3 barycentricCoords = float3(1.f - evalParams.barycentricCoords.x - evalParams.barycentricCoords.y, evalParams.barycentricCoords.x, evalParams.barycentricCoords.y);

	uint3 indices;
	[unroll]
	for (uint idx = 0; idx < 3; ++idx)
	{
		indices[idx] = UGB().LoadVertexIndex(instanceData.indicesDataUGBOffset, evalParams.triangleIdx * 3 + idx);
	}

	float2 normalizedUV = 0.f;
	[unroll]
	for (uint idx = 0; idx < 3; ++idx)
	{
		normalizedUV += UGB().LoadNormalizedUV(instanceData.uvsDataUGBOffset, indices[idx]) * barycentricCoords[idx];
	}
	const float2 uv = instanceData.uvsMin + normalizedUV * instanceData.uvsRange;

	const SPT_MATERIAL_DATA_TYPE materialData = LoadMaterialData(instanceData.materialDataHandle);

	MaterialEvaluationParameters materialEvalParams;
	materialEvalParams.clipSpace     = -1.f;
	materialEvalParams.hasTangent    = false;
	materialEvalParams.uv            = uv;
	materialEvalParams.worldLocation = evalParams.hitLocationWS;

	const CustomOpacityOutput opacityOutput = EvaluateCustomOpacity(materialEvalParams, materialData);

	return opacityOutput;
}

#else  // SPT_MATERIAL_SHADER_PATH

MaterialEvaluationOutput EvaluateMat(in RTMaterialEvaluationParams evalParams)
{
	return (MaterialEvaluationOutput)0;
}

CustomOpacityOutput EvaluateOpacity(in RTMaterialEvaluationParams evalParams)
{
	return (CustomOpacityOutput)0;
}

#endif // SPT_MATERIAL_SHADER_PATH

} // RTMaterial

#endif // RAY_TRACING_MATERIALS_HLSLI
