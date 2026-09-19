#include "SculptorShader.hlsli"

[[shader_params(DOFShaderParameters, PARAMS_D_O_F_GENERATE_CO_C)]]
[[shader_params(GPURenderView, VIEW)]]

#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void DOFGenerateCoCCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;
    
    uint2 outputRes = PARAMS_D_O_F_GENERATE_CO_C->cocTexture.GetResolution();

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 uv = (pixel + 0.5f) / float2(outputRes);
        
        const float ndcDepth = PARAMS_D_O_F_GENERATE_CO_C->depthTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv, 0.f);
        const float linearDepth = ComputeLinearDepth(ndcDepth, VIEW->sceneView);

        const float nearFieldCoC = linearDepth < PARAMS_D_O_F_GENERATE_CO_C->nearFieldEnd ? 1.f - smoothstep(PARAMS_D_O_F_GENERATE_CO_C->nearFieldBegin, PARAMS_D_O_F_GENERATE_CO_C->nearFieldEnd, linearDepth) : 0.f;
        const float farFieldCoC = linearDepth > PARAMS_D_O_F_GENERATE_CO_C->farFieldBegin ? smoothstep(PARAMS_D_O_F_GENERATE_CO_C->farFieldBegin, PARAMS_D_O_F_GENERATE_CO_C->farFieldEnd, linearDepth) : 0.f;
        
        PARAMS_D_O_F_GENERATE_CO_C->cocTexture[pixel] = float2(nearFieldCoC, farFieldCoC);
    }
}
