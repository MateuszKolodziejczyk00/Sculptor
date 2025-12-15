#ifndef SPT_MATERIAL_TYPES_HLSLI
#define SPT_MATERIAL_TYPES_HLSLI


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


struct MaterialEvaluationOutput
{
	float3  shadingNormal;
	float3  geometryNormal;

	float   roughness;

	float3  baseColor;
	float   metallic;

	float3 emissiveColor;
};

#endif // SPT_MATERIAL_TYPES_HLSLI
