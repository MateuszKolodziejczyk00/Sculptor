#ifndef MATERIAL_SYSTEM_HLSLI
#define MATERIAL_SYSTEM_HLSLI


struct MaterialEvaluationParameters
{
	float3  normal;

	float3  tangent;
	float3  bitangent;
	bool    hasTangent;

#ifdef SPT_MATERIAL_SAMPLE_CUSTOM_DERIVATIVES
	TextureCoord uv;
#else
	float2  uv;
#endif // SPT_MATERIAL_SAMPLE_CUSTOM_DERIVATIVES

	float3  worldLocation;
	
	float4  clipSpace;
};


struct CustomOpacityOutput
{
	bool shouldDiscard;
};


/* 
 * Custom opacity function signature:
 * CustomOpacityOutput EvaluateCustomOpacity(MaterialEvaluationParameters evalParams, SPT_MATERIAL_DATA_TYPE materialData);
 */


struct MaterialEvaluationOutput
{
	float3  shadingNormal;
	float3  geometryNormal;

	float   roughness;

	float3  baseColor;
	float   metallic;

	float3 emissiveColor;
};


/* 
 * Material evaluation function signature:
 * MaterialEvaluationOutput EvaluateMaterial(MaterialEvaluationParameters evalParams, SPT_MATERIAL_DATA_TYPE materialData);
 */


#if defined(SPT_MATERIAL_SAMPLE_EXPLICIT_LEVEL)

#define SPT_MATERIAL_SAMPLE(sampler, uv) SampleLevel(sampler, uv, SPT_MATERIAL_SAMPLE_EXPLICIT_LEVEL)
#define SPT_MATERIAL_SAMPLE_LEVEL(sampler, uv, level) SampleLevel(sampler, uv, level)

#elif defined(SPT_MATERIAL_SAMPLE_CUSTOM_DERIVATIVES)

#define SPT_MATERIAL_SAMPLE(sampler, coord) SampleGrad(sampler, coord.uv, coord.duv_dx, coord.duv_dy)
#define SPT_MATERIAL_SAMPLE_LEVEL(sampler, coord, level) SampleLevel(sampler, coord.uv, level)

#else

#define SPT_MATERIAL_SAMPLE(sampler, uv) Sample(sampler, uv)
#define SPT_MATERIAL_SAMPLE_LEVEL(sampler, uv, level) SampleLevel(sampler, uv, level)

#endif


[[shader_struct(SPT_MATERIAL_DATA_TYPE)]]


#ifdef DS_MaterialsDS

#define SPT_MATERIAL_DATA_ALIGNMENT 32

template<typename TMaterialData>
TMaterialData LoadMaterialDataInternal(in uint16_t materialDataID)
{
	const uint materialDataOffset = uint(materialDataID) * SPT_MATERIAL_DATA_ALIGNMENT;
	return u_materialsData.Load<TMaterialData>(materialDataOffset);
}

SPT_MATERIAL_DATA_TYPE LoadMaterialData(in uint16_t materialDataID)
{
	return LoadMaterialDataInternal<SPT_MATERIAL_DATA_TYPE>(materialDataID);
}

#endif // DS_MaterialsDS

#endif // MATERIAL_SYSTEM_HLSLI