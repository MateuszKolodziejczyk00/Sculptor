#include "SculptorShader.hlsli"

[[descriptor_set(PrepareUnifiedDenoisingGuidingTexturesDS, 0)]]

#include "Utils/GBuffer/GBuffer.hlsli"
#include "Shading/Shading.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void PrepareUnifiedDenoisingGuidingTexturesCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;

    const float3 normal = DecodeGBufferNormal(u_tangentFrame.Load(uint3(pixel, 0u)));

    const float4 baseColorMetallic = u_baseColorMetallic.Load(uint3(pixel, 0u));

    float3 diffuseColor;
    float3 specularColor;
    ComputeSurfaceColor(baseColorMetallic.rgb, baseColorMetallic.w, OUT diffuseColor, OUT specularColor);

	u_rwDiffuseAlbedo[pixel]  = diffuseColor;
	u_rwSpecularAlbedo[pixel] = specularColor;
	u_rwNormals[pixel]        = normal;
}
