

struct MaterialEvaluationParameters
{
	float3  normal;

	float3  tangent;
	float3  bitangent;
	bool    hasTangent;

	float2  uv;

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


#ifdef SPT_MATERIAL_SAMPLE_EXPLICIT_LEVEL

#define SPT_MATERIAL_SAMPLE(sampler, uv) SampleLevel(sampler, uv, SPT_MATERIAL_SAMPLE_EXPLICIT_LEVEL)

#else

#define SPT_MATERIAL_SAMPLE(sampler, uv) Sample(sampler, uv)

#endif // SPT_MATERIAL_SAMPLE_EXPLICIT_LEVEL


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