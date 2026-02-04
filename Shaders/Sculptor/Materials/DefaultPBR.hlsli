#ifndef DEFAULT_PBR_HLSLI
#define DEFAULT_PBR_HLSLI

#include "SceneRendering/GPUScene.hlsli"
#include "Utils/Packing.hlsli"


CustomOpacityOutput EvaluateCustomOpacity(MaterialEvaluationParameters evalParams, SPT_MATERIAL_DATA_TYPE materialData)
{
    CustomOpacityOutput output;

    if(materialData.alphaTexture.IsValid())
    {
        float opacity = 1.f;
        opacity = materialData.alphaTexture.SPT_MATERIAL_SAMPLE(BindlessSamplers::MaterialLinear(), evalParams.uv);
        output.shouldDiscard = opacity < 0.8f;
    }
    else
    {
        output.shouldDiscard = false;
    }

    return output;
}


MaterialEvaluationOutput EvaluateMaterial(MaterialEvaluationParameters evalParams, SPT_MATERIAL_DATA_TYPE materialData)
{
    float3 baseColor = 1.f;
    
	if (materialData.baseColorTexture.IsValid())
    {
        baseColor = materialData.baseColorTexture.SPT_MATERIAL_SAMPLE(BindlessSamplers::MaterialAniso(), evalParams.uv).rgb;
    }

    baseColor *= materialData.baseColorFactor;

    float metallic = materialData.metallicFactor;
    float roughness = materialData.roughnessFactor;

    if(materialData.metallicRoughnessTexture.IsValid())
    {
		const float2 metallicRoughness = materialData.metallicRoughnessTexture.SPT_MATERIAL_SAMPLE(BindlessSamplers::MaterialAniso(), evalParams.uv);
		metallic *= metallicRoughness.x;
		roughness *= metallicRoughness.y;
    }

    float3 shadingNormal;
    const float3 geometryNormal = normalize(evalParams.normal);
    if(evalParams.hasTangent && materialData.normalsTexture.IsValid())
    {
		const float2 textureNormal = materialData.normalsTexture.SPT_MATERIAL_SAMPLE(BindlessSamplers::MaterialAniso(), evalParams.uv);

		const float3 tangentNormal = UnpackTangentNormalFromXY(textureNormal);
        const float3x3 TBN = transpose(float3x3(normalize(evalParams.tangent), normalize(evalParams.bitangent), geometryNormal));
        shadingNormal = normalize(mul(TBN, tangentNormal));
    }
    else
    {
        shadingNormal = geometryNormal;
    }

    float3 emissiveColor = materialData.emissiveFactor;
	if (materialData.emissiveTexture.IsValid())
    {
        emissiveColor *= materialData.emissiveTexture.SPT_MATERIAL_SAMPLE(BindlessSamplers::MaterialAniso(), evalParams.uv);
    }

    MaterialEvaluationOutput output;

    output.shadingNormal  = shadingNormal;
    output.geometryNormal = geometryNormal;
    output.roughness      = roughness;
    output.baseColor      = baseColor;
    output.metallic       = metallic;
    output.emissiveColor  = emissiveColor;

    return output;
}

#endif // DEFAULT_PBR_HLSLI
