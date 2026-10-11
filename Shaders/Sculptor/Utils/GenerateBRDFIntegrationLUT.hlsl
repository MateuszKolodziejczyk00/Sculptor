#include "SculptorShader.hlsli"

[[shader_params(BRDFIntegrationLUTGenerationConstants, PARAMS_B_R_D_F_INTEGRATION_L_U_T_GENERATION)]]

#include "Shading/Shading.hlsli"


float GeometrySchlickGGX(in float NdotV, in float a)
{
	const float k = (a * a) / 2.0;

	const float nom   = NdotV;
	const float denom = NdotV * (1.0 - k) + k;

	return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
	const float NdotV = max(dot(N, V), 0.0);
	const float NdotL = max(dot(N, L), 0.0);
	const float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	const float ggx1 = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
} 


float3 ImportanceSampleGGX(in float2 Xi, in float3 N, in float roughness)
{
	float a = roughness*roughness;
	
	float phi = 2.0 * PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
	
	// from spherical coordinates to cartesian coordinates
	float3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;
	
	// from tangent-space floattor to world-space sample floattor
	float3 up        = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
	float3 tangent   = normalize(cross(up, N));
	float3 bitangent = cross(N, tangent);
	
	float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}


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
    
    uint2 outputRes = PARAMS_B_R_D_F_INTEGRATION_L_U_T_GENERATION->lut.GetResolution();

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 uv = (pixel + 0.5f) / outputRes;

        PARAMS_B_R_D_F_INTEGRATION_L_U_T_GENERATION->lut[pixel] = IntegrateBRDF(uv.x, uv.y);
    }
}
