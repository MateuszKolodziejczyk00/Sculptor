#include "SculptorShader.hlsli"
#include "Shading/Shading.hlsli"

[[descriptor_set(BRDFIntegrationLUTGenerationDS, 0)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


float2 IntegrateBRDF(float NdotV, float roughness)
{
    float3 viewDir;
    viewDir.x = sqrt(1.f - NdotV * NdotV);
    viewDir.y = 0.0;
    viewDir.z = NdotV;

    float scale = 0.f;
    float bias = 0.f;

    const float3 normal = float3(0.f, 0.f, 1.f);

    const uint SAMPLE_COUNT = 1024u;
    for(uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        const float2 random = Hammersley(i, SAMPLE_COUNT);
        const float3 H = ImportanceSampleGGX(random, normal, roughness);
        const float3 L = reflect(-viewDir, H);

        const float NdotL = max(L.z, 0.0);
        const float NdotH = max(H.z, 0.0);
        const float VdotH = max(dot(viewDir, H), 0.0);

        if(NdotL > 0.f)
        {
            const float G = GeometrySmith(normal, viewDir, L, roughness);
            const float G_Vis = (G * VdotH) / (NdotH * NdotV);
            const float Fc = pow(1.0 - VdotH, 5.0);

            scale += (1.f - Fc) * G_Vis;
            bias += Fc * G_Vis;
        }
    }

    const float rcpSampleCount = rcp(float(SAMPLE_COUNT));

    scale *= rcpSampleCount;
    bias  *= rcpSampleCount;

    return float2(scale, bias);
}


[numthreads(8, 8, 1)]
void GenerateBRDFIntegrationLUTCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;
    
    uint2 outputRes;
    u_lut.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 uv = (pixel + 0.5f) / outputRes;

        u_lut[pixel] = IntegrateBRDF(uv.x, uv.y);
    }
}
