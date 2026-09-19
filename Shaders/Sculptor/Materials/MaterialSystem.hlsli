#ifndef MATERIAL_SYSTEM_HLSLI
#define MATERIAL_SYSTEM_HLSLI

#include "Materials/MaterialTypes.hlsli"
#include "SceneRendering/GPUMaterials.hlsli"


interface IMaterialSampler
{
	T Sample<T : ITexelElement & IWeightable>(in SRVTexture2D<T> texture, SamplerState sampler, in float2 uv);
	T SampleLevel<T : ITexelElement & IWeightable>(in SRVTexture2D<T> texture, SamplerState sampler, in float2 uv, in float level);
	T SampleGrad<T : ITexelElement & IWeightable>(in SRVTexture2D<T> texture, SamplerState sampler, in float2 uv, in float2 ddx, in float2 ddy);
};


struct DefaultMaterialSampler : IMaterialSampler
{
	static DefaultMaterialSampler Initialize(MaterialEvaluationParameters evalParams)
	{
		DefaultMaterialSampler sampler;
		return sampler;
	}

	T Sample<T : ITexelElement & IWeightable>(in SRVTexture2D<T> texture, SamplerState sampler, in float2 uv)
	{
		return texture.Sample(sampler, uv);
	}

	T SampleLevel<T : ITexelElement & IWeightable>(in SRVTexture2D<T> texture, SamplerState sampler, in float2 uv, in float level)
	{
		return texture.SampleLevel(sampler, uv, level);
	}

	T SampleGrad<T : ITexelElement & IWeightable>(in SRVTexture2D<T> texture, SamplerState sampler, in float2 uv, in float2 ddx, in float2 ddy)
	{
		return texture.SampleGrad(sampler, uv, ddx, ddy);
	}
};


#ifdef SPT_MATERIAL_DATA_TYPE
[[shader_struct(SPT_MATERIAL_DATA_TYPE)]]
#endif // SPT_MATERIAL_DATA_TYPE


#ifndef SPT_MATERIAL_DATA_TYPE
#define SPT_MATERIAL_DATA_TYPE uint

/* 
 * Custom opacity function signature:
 * template<typename TSampler>
 * CustomOpacityOutput EvaluateCustomOpacity(TSampler sampler, MaterialEvaluationParameters evalParams, SPT_MATERIAL_DATA_TYPE materialData);
 */



/* 
 * Material evaluation function signature:
 * template<typename TSampler>
 * MaterialEvaluationOutput EvaluateMaterial(TSampler sampler, MaterialEvaluationParameters evalParams, SPT_MATERIAL_DATA_TYPE materialData);
 */

CustomOpacityOutput EvaluateCustomOpacity<TSampler : IMaterialSampler>(TSampler sampler, MaterialEvaluationParameters evalParams, SPT_MATERIAL_DATA_TYPE materialData)
{
	CustomOpacityOutput output;
	output.shouldDiscard = false;
	return output;
}


MaterialEvaluationOutput EvaluateMaterial<TSampler : IMaterialSampler>(TSampler sampler, MaterialEvaluationParameters evalParams, SPT_MATERIAL_DATA_TYPE materialData)
{
	MaterialEvaluationOutput output;
	return output;
}

#endif // SPT_MATERIAL_DATA_TYPE

#if defined(SPT_MATERIAL_SAMPLE_EXPLICIT_LEVEL)
#define SPT_SAMPLER_ANISO_IF_NOT_EXPLICIT_LEVEL BindlessSamplers::LinearRepeat()
#else
#define SPT_SAMPLER_ANISO_IF_NOT_EXPLICIT_LEVEL BindlessSamplers::MaterialAniso()
#endif // SPT_MATERIAL_SAMPLE_EXPLICIT_LEVEL


#if defined(SPT_MATERIAL_SAMPLE_EXPLICIT_LEVEL)

#define SPT_MATERIAL_SAMPLE(texture, sampler, uv) SampleLevel(texture, sampler, uv, SPT_MATERIAL_SAMPLE_EXPLICIT_LEVEL)
#define SPT_MATERIAL_SAMPLE_LEVEL(texture, sampler, uv, level) SampleLevel(texture, sampler, uv, level)

#elif defined(SPT_MATERIAL_SAMPLE_CUSTOM_DERIVATIVES)

#define SPT_MATERIAL_SAMPLE(texture, sampler, coord) SampleGrad(texture, sampler, coord.uv, coord.duv_dx, coord.duv_dy)
#define SPT_MATERIAL_SAMPLE_LEVEL(texture, sampler, coord, level) SampleLevel(texture, sampler, coord.uv, level)

#else

#define SPT_MATERIAL_SAMPLE(texture, sampler, uv) Sample(texture, sampler, uv)
#define SPT_MATERIAL_SAMPLE_LEVEL(texture, sampler, uv, level) SampleLevel(texture, sampler, uv, level)

#endif


[[shader_struct(MaterialDataHandle)]]

#ifdef SPT_MATERIAL_SHADER_PATH
#include SPT_MATERIAL_SHADER_PATH
#endif // SPT_MATERIAL_SHADER_PATH

#define SPT_MATERIAL_DATA_ALIGNMENT 32


[[shader_struct(MaterialUnifiedData)]]


TMaterialData LoadMaterialData<TMaterialData>(in MaterialUnifiedData materialsData, in MaterialDataHandle materialDataHandle)
{
	const uint materialDataOffset = uint(materialDataHandle.id) * SPT_MATERIAL_DATA_ALIGNMENT;
	return materialsData.materialsData.Load<TMaterialData>(materialDataOffset);
}

TMaterialFeature LoadMaterialFeature<TMaterialFeature>(in MaterialUnifiedData materialsData, in MaterialDataHandle materialDataHandle, in uint16_t featureID)
{
	const uint materialFeatureOffset = uint(materialDataHandle.id) * SPT_MATERIAL_DATA_ALIGNMENT + uint(featureID);
	return materialsData.materialsData.Load<TMaterialFeature>(materialFeatureOffset);
}

#ifdef SPT_MATERIAL_DATA_TYPE
CustomOpacityOutput EvaluateCustomOpacity(MaterialEvaluationParameters evalParams, SPT_MATERIAL_DATA_TYPE materialData)
{
	DefaultMaterialSampler sampler = DefaultMaterialSampler::Initialize(evalParams);
	return EvaluateCustomOpacity(sampler, evalParams, materialData);
}

MaterialEvaluationOutput EvaluateMaterial(MaterialEvaluationParameters evalParams, SPT_MATERIAL_DATA_TYPE materialData)
{
	DefaultMaterialSampler sampler = DefaultMaterialSampler::Initialize(evalParams);
	return EvaluateMaterial(sampler, evalParams, materialData);
}
#endif // SPT_MATERIAL_DATA_TYPE


#ifdef PARAM_RenderSceneConstants

TMaterialData LoadMaterialDataInternal<TMaterialData>(in MaterialDataHandle materialDataHandle)
{
	return LoadMaterialData<TMaterialData>(GPUMaterials().data, materialDataHandle);
}

#ifdef SPT_MATERIAL_DATA_TYPE
SPT_MATERIAL_DATA_TYPE LoadMaterialData(in MaterialDataHandle materialDataHandle)
{
	return LoadMaterialDataInternal<SPT_MATERIAL_DATA_TYPE>(materialDataHandle);
}
#endif // SPT_MATERIAL_DATA_TYPE

TMaterialFeature LoadMaterialFeature<TMaterialFeature>(in MaterialDataHandle materialDataHandle, in uint16_t featureID)
{
	return LoadMaterialFeature<TMaterialFeature>(GPUMaterials().data, materialDataHandle, featureID);
}

#endif // PARAM_RenderSceneConstants

#endif // MATERIAL_SYSTEM_HLSLI
