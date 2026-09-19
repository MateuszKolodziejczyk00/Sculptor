#include "SculptorShader.hlsli"

[[shader_params(DOFCompositeParams, PARAMS_D_O_F_COMPOSITE)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void DOFCompositeCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;
    
    uint2 outputRes = PARAMS_D_O_F_COMPOSITE->resultTexture.GetResolution();

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 pixelSize = 1.f / float2(outputRes);
        
        const float2 uv = (pixel + 0.5f) * pixelSize;

        // Far

        const float2 uv00 = uv;
        const float2 uv10 = uv + float2(pixelSize.x, 0.f);
        const float2 uv01 = uv + float2(0.f, pixelSize.y);
        const float2 uv11 = uv + pixelSize.x;

        const float cocFar          = PARAMS_D_O_F_COMPOSITE->cocTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv, 0).y;
        const float4 cocFar4        = PARAMS_D_O_F_COMPOSITE->cocTexture.GatherGreen(BindlessSamplers::NearestClampEdge(), uv).wzxy;
        const float4 cocFarDiffs    = abs(cocFar4 - cocFar);

        const float3 dofFar00 = PARAMS_D_O_F_COMPOSITE->farFieldDOFTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv00, 0).xyz;
        const float3 dofFar10 = PARAMS_D_O_F_COMPOSITE->farFieldDOFTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv10, 0).xyz;
        const float3 dofFar01 = PARAMS_D_O_F_COMPOSITE->farFieldDOFTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv01, 0).xyz;
        const float3 dofFar11 = PARAMS_D_O_F_COMPOSITE->farFieldDOFTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv11, 0).xyz;
        
        const float a = 0.25f;
        const float b = 0.25f;
        const float c = 0.25f;
        const float d = 0.25f;

        float3 dofFar = 0.f;
        float weightsSum = 0.f;
        
        const float weight00 = a / (cocFarDiffs.x + 0.0001f);
        dofFar += dofFar00 * weight00;
        weightsSum += weight00;

        const float weight10 = b / (cocFarDiffs.y + 0.0001f);
        dofFar += dofFar10 * weight10;
        weightsSum += weight10;

        const float weight01 = c / (cocFarDiffs.z + 0.0001f);
        dofFar += dofFar01 * weight01;
        weightsSum += weight01;

        const float weight11 = d / (cocFarDiffs.w + 0.0001f);
        dofFar += dofFar11 * weight11;
        weightsSum += weight11;

        dofFar /= weightsSum;

        const float blend = 1.0f;

        float3 result = PARAMS_D_O_F_COMPOSITE->resultTexture[pixel.xy];

        result = lerp(dofFar, result, Pow2(1.f - saturate(cocFar * blend)));

        // Near

        const float cocNear = PARAMS_D_O_F_COMPOSITE->nearCoCBlurredTexture.SampleLevel(BindlessSamplers::LinearClampEdge(), uv, 0.f);

        const float3 dofNear = PARAMS_D_O_F_COMPOSITE->nearFieldDOFTexture.SampleLevel(BindlessSamplers::LinearClampEdge(), uv, 0).xyz;

        result = lerp(result, dofNear, saturate(cocNear * blend));

        PARAMS_D_O_F_COMPOSITE->resultTexture[pixel.xy] = result;
    }
}
