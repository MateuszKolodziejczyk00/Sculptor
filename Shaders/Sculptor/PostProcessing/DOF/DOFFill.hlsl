#include "SculptorShader.hlsli"

[[shader_params(DOFFillPassParams, PARAMS_D_O_F_FILL_PASS)]]

#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void DOFFillCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;
    
    uint2 outputRes = PARAMS_D_O_F_FILL_PASS->nearFieldFilledDOFTexture.GetResolution();

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 pixelSize = 1.0f / float2(outputRes);
        const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

        const float cocFar  = PARAMS_D_O_F_FILL_PASS->cocTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv, 0).y;
        const float cocNear = PARAMS_D_O_F_FILL_PASS->cocNearBlurredTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv, 0);

        float3 dofNear = PARAMS_D_O_F_FILL_PASS->nearFieldDOFTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv, 0);

        if(cocNear > 0.f)
        {
            [unroll]
            for (int y = -1; y <= 1; ++y)
            {
                [unroll]
                for (int x = -1; x <= 1; ++x)
                {
                    const float2 sampleUV = uv + float2(x, y) * pixelSize;
                    dofNear = max(dofNear, PARAMS_D_O_F_FILL_PASS->nearFieldDOFTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), sampleUV, 0));
                }
            }
        }

        float3 dofFar = PARAMS_D_O_F_FILL_PASS->farFieldDOFTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv, 0);

        if(cocFar > 0.f)
        {
            [unroll]
            for (int y = -1; y <= 1; ++y)
            {
                [unroll]
                for (int x = -1; x <= 1; ++x)
                {
                    const float2 sampleUV = uv + float2(x, y) * pixelSize;
                    dofFar = max(dofFar, PARAMS_D_O_F_FILL_PASS->farFieldDOFTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), sampleUV, 0));
                }
            }
        }
	
        PARAMS_D_O_F_FILL_PASS->nearFieldFilledDOFTexture[pixel] = dofNear;
        PARAMS_D_O_F_FILL_PASS->farFieldFilledDOFTexture[pixel] = dofFar;
    }
}
