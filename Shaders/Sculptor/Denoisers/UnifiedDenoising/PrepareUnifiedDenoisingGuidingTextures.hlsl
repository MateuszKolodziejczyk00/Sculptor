#include "SculptorShader.hlsli"

[[descriptor_set(PrepareUnifiedDenoisingGuidingTexturesDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/GBuffer/GBuffer.hlsli"
#include "Shading/Shading.hlsli"
#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


// Source: DLSS-RR Integration Guide API, Appendix 1
float3 EnvBRDFApprox2(float3 SpecularColor, float alpha, float NoV)
{
    NoV = abs(NoV);
    // [Ray Tracing Gems, Chapter 32]
    float4 X;
    X.x = 1.f;
    X.y = NoV;
    X.z = NoV * NoV;
    X.w = NoV * X.z;
    float4 Y;
    Y.x = 1.f;
    Y.y = alpha;
    Y.z = alpha * alpha;
    Y.w = alpha * Y.z;

    float2x2 M1 = float2x2(0.99044f, -1.28514f, 
                           1.29678f, -0.755907f);

    float3x3 M2 = float3x3(1.f, 2.92338f, 59.4188f,
                           20.3225f, -27.0302f, 222.592f,
                           121.563f, 626.13f, 316.627f);

    float2x2 M3 = float2x2(0.0365463f, 3.32707,
                           9.0632f, -9.04756);

    float3x3 M4 = float3x3(1.f, 3.59685f, -1.36772f,
                           9.04401f, -16.3174f, 9.22949f,
                           5.56589f, 19.7886f, -20.2123f);
                           
    float bias = dot(mul(M1, X.xy), Y.xy) * rcp(dot(mul(M2, X.xyw), Y.xyw));
    float scale = dot(mul(M3, X.xy), Y.xy) * rcp(dot(mul(M4, X.xzw), Y.xyw));

    // This is a hack for specular reflectance of 0
    bias *= saturate(SpecularColor.g * 50);
    return mad(SpecularColor, max(0, scale), max(0, bias));
}



[numthreads(8, 8, 1)]
void PrepareUnifiedDenoisingGuidingTexturesCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;

    const float3 normal = DecodeGBufferNormal(u_tangentFrame.Load(uint3(pixel, 0u)));

    const float4 baseColorMetallic = u_baseColorMetallic.Load(uint3(pixel, 0u));

    float3 diffuseColor;
    float3 specularColor;
    ComputeSurfaceColor(baseColorMetallic.rgb, baseColorMetallic.w, OUT diffuseColor, OUT specularColor);

    const float depth = u_depth.Load(uint3(pixel, 0u));
    const float2 uv = (pixel + 0.5f) * u_constants.rcpResolution;

    const float3 worldLocation = NDCToWorldSpace(float3(uv * 2.f - 1.f, depth), u_sceneView);

    const float3 toView = normalize(u_sceneView.viewLocation - worldLocation);
    const float dotNV = dot(toView, normal);

    const float roughness = u_roughness.Load(uint3(pixel, 0u));

    const float3 outSpecularColor = EnvBRDFApprox2(specularColor, RoughnessToAlpha(roughness), dotNV);

	u_rwDiffuseAlbedo[pixel]  = diffuseColor;
	u_rwSpecularAlbedo[pixel] = outSpecularColor;
	u_rwNormals[pixel]        = normal;
}
