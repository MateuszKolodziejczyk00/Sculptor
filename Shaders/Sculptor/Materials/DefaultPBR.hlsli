

CustomOpacityOutput EvaluateCustomOpacity(MaterialEvaluationParameters evalParams, SPT_MATERIAL_DATA_TYPE materialData)
{
    CustomOpacityOutput output;

    if(materialData.baseColorTextureIdx != IDX_NONE_32)
    {
        float opacity = 1.f;
        opacity = u_materialsTextures[materialData.baseColorTextureIdx].SPT_MATERIAL_SAMPLE(u_materialLinearSampler, evalParams.uv).a;
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
    
    if(materialData.baseColorTextureIdx != IDX_NONE_32)
    {
        baseColor = u_materialsTextures[NonUniformResourceIndex(materialData.baseColorTextureIdx)].SPT_MATERIAL_SAMPLE(u_materialAnisoSampler, evalParams.uv).rgb;
        baseColor = pow(baseColor, 2.2f);
    }

    baseColor *= materialData.baseColorFactor;

    float metallic = materialData.metallicFactor;
    float roughness = materialData.roughnessFactor;
    
    if(materialData.metallicRoughnessTextureIdx != IDX_NONE_32)
    {
        const float2 metallicRoughness = u_materialsTextures[NonUniformResourceIndex(materialData.metallicRoughnessTextureIdx)].SPT_MATERIAL_SAMPLE(u_materialAnisoSampler, evalParams.uv).xy;
        metallic *= metallicRoughness.x;
        roughness *= metallicRoughness.y;
    }

    float3 shadingNormal;
    const float3 geometryNormal = normalize(evalParams.normal);
    if(evalParams.hasTangent && materialData.normalsTextureIdx != IDX_NONE_32)
    {
        float3 textureNormal = u_materialsTextures[NonUniformResourceIndex(materialData.normalsTextureIdx)].SPT_MATERIAL_SAMPLE(u_materialAnisoSampler, evalParams.uv).xyz;
        textureNormal = textureNormal * 2.f - 1.f;
        textureNormal.z = sqrt(1.f - saturate(Pow2(textureNormal.x) + Pow2(textureNormal.y)));
        const float3x3 TBN = transpose(float3x3(normalize(evalParams.tangent), normalize(evalParams.bitangent), geometryNormal));
        shadingNormal = normalize(mul(TBN, textureNormal));
    }
    else
    {
        shadingNormal = geometryNormal;
    }

    float3 emissiveColor = materialData.emissiveFactor;
    if(materialData.emissiveTextureIdx != IDX_NONE_32)
    {
        emissiveColor *= u_materialsTextures[NonUniformResourceIndex(materialData.emissiveTextureIdx)].SPT_MATERIAL_SAMPLE(u_materialAnisoSampler, evalParams.uv).rgb;
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
