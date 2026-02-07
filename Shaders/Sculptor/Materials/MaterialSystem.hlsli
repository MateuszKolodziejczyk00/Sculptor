#ifndef MATERIAL_SYSTEM_HLSLI
#define MATERIAL_SYSTEM_HLSLI

#include "Materials/MaterialTypes.hlsli"
#include "SceneRendering/GPUMaterials.hlsli"


/* 
 * Custom opacity function signature:
 * CustomOpacityOutput EvaluateCustomOpacity(MaterialEvaluationParameters evalParams, SPT_MATERIAL_DATA_TYPE materialData);
 */



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


[[shader_struct(MaterialDataHandle)]]


#ifdef SPT_MATERIAL_DATA_TYPE
[[shader_struct(SPT_MATERIAL_DATA_TYPE)]]
#endif // SPT_MATERIAL_DATA_TYPE

#ifdef SPT_MATERIAL_SHADER_PATH
#include SPT_MATERIAL_SHADER_PATH
#endif // SPT_MATERIAL_SHADER_PATH

#define SPT_MATERIAL_DATA_ALIGNMENT    32
#define SPT_MATERIAL_FEATURE_ALIGNMENT 16

#ifdef DS_RenderSceneDS

template<typename TMaterialData>
TMaterialData LoadMaterialDataInternal(in MaterialDataHandle materialDataHandle)
{
	const uint materialDataOffset = uint(materialDataHandle.id) * SPT_MATERIAL_DATA_ALIGNMENT;
	return GPUMaterials().data.materialsData.Load<TMaterialData>(materialDataOffset);
}

#ifdef SPT_MATERIAL_DATA_TYPE
SPT_MATERIAL_DATA_TYPE LoadMaterialData(in MaterialDataHandle materialDataHandle)
{
	return LoadMaterialDataInternal<SPT_MATERIAL_DATA_TYPE>(materialDataHandle);
}
#endif // SPT_MATERIAL_DATA_TYPE

template<typename TMaterialFeature>
TMaterialFeature LoadMaterialFeature(in MaterialDataHandle materialDataHandle, in uint16_t featureID)
{
	const uint materialFeatureOffset = uint(materialDataHandle.id) * SPT_MATERIAL_DATA_ALIGNMENT + uint(featureID) * SPT_MATERIAL_FEATURE_ALIGNMENT;
	return GPUMaterials().data.materialsData.Load<TMaterialFeature>(materialFeatureOffset);
}

#endif // DS_RenderSceneDS

#endif // MATERIAL_SYSTEM_HLSLI
