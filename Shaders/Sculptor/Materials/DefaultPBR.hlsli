

[[shader_struct(MaterialPBRData)]]


MaterialEvaluationOutput EvaluateMaterial(MaterialEvaluationParameters evalParams, SPT_MATERIAL_DATA_TYPE materialData)
{
    float3 baseColor = 1.f;
    
    if(materialData.baseColorTextureIdx != IDX_NONE_32)
    {
        baseColor = u_materialsTextures[materialData.baseColorTextureIdx].SPT_MATERIAL_SAMPLE(u_materialTexturesSampler, evalParams.uv).rgb;
        baseColor = pow(baseColor, 2.2f);
    }

    baseColor *= materialData.baseColorFactor;

    float metallic = materialData.metallicFactor;
    float roughness = materialData.roughnessFactor;
    
    if(materialData.metallicRoughnessTextureIdx != IDX_NONE_32)
    {
        float2 metallicRoughness = u_materialsTextures[materialData.metallicRoughnessTextureIdx].SPT_MATERIAL_SAMPLE(u_materialTexturesSampler, evalParams.uv).xy;
        metallic *= metallicRoughness.x;
        roughness *= metallicRoughness.y;
    }

    float3 shadingNormal;
    const float3 geometryNormal = normalize(evalParams.normal);
    if(evalParams.hasTangent && materialData.normalsTextureIdx != IDX_NONE_32)
    {
        float3 textureNormal = u_materialsTextures[materialData.normalsTextureIdx].SPT_MATERIAL_SAMPLE(u_materialTexturesSampler, evalParams.uv).xyz;
        textureNormal = textureNormal * 2.f - 1.f;
        textureNormal.z = sqrt(1.f - saturate(Pow2(textureNormal.x) + Pow2(textureNormal.y)));
        const float3x3 TBN = transpose(float3x3(normalize(evalParams.tangent), normalize(evalParams.bitangent), geometryNormal));
        shadingNormal = normalize(mul(TBN, textureNormal));
    }
    else
    {
        shadingNormal = normalize(evalParams.normal);
    }

    MaterialEvaluationOutput output;

    output.shadingNormal = shadingNormal;
    output.geometryNormal = geometryNormal;
    output.roughness = roughness;
    output.baseColor = baseColor;
    output.metallic = metallic;

    return output;
}