

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
    float opacity;
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