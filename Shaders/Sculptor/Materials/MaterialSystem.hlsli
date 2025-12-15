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


#ifdef SPT_MATERIAL_DATA_TYPE
[[shader_struct(SPT_MATERIAL_DATA_TYPE)]]
#endif // SPT_MATERIAL_DATA_TYPE

#ifdef SPT_MATERIAL_SHADER_PATH
#include SPT_MATERIAL_SHADER_PATH
#endif // SPT_MATERIAL_SHADER_PATH

#define SPT_MATERIAL_DATA_ALIGNMENT 32

#ifdef DS_RenderSceneDS

template<typename TMaterialData>
TMaterialData LoadMaterialDataInternal(in uint16_t materialDataID)
{
	const uint materialDataOffset = uint(materialDataID) * SPT_MATERIAL_DATA_ALIGNMENT;
	return GPUMaterials().data.materialsData.Load<TMaterialData>(materialDataOffset);
}

#ifdef SPT_MATERIAL_DATA_TYPE
SPT_MATERIAL_DATA_TYPE LoadMaterialData(in uint16_t materialDataID)
{
	return LoadMaterialDataInternal<SPT_MATERIAL_DATA_TYPE>(materialDataID);
}
#endif // SPT_MATERIAL_DATA_TYPE
#endif // DS_RenderSceneDS

#endif // MATERIAL_SYSTEM_HLSLI