#include "SculptorShader.hlsli"

[[shader_params(DOComputationPassParams, PARAMS_D_O_COMPUTATION_PASS)]]

#include "Utils/SceneViewUtils.hlsli"


#define DOF_SAMPLES_NUM 49

static const float2 dofSamples[DOF_SAMPLES_NUM] =
{
    float2(0.000, 0.000),
    float2(0.000, 2.000),
    float2(1.414, 1.414),
    float2(2.000, 0.000),
    float2(1.414, -1.414),
    float2(0.000, -2.000),
    float2(-1.414, -1.414),
    float2(-2.000, 0.000),
    float2(-1.414, 1.414),
    float2(0.000, 4.000),
    float2(1.531, 3.696),
    float2(2.828, 2.828),
    float2(3.696, 1.531),
    float2(4.000, 0.000),
    float2(3.696, -1.531),
    float2(2.828, -2.828),
    float2(1.531, -3.696),
    float2(0.000, -4.000),
    float2(-1.531, -3.696),
    float2(-2.828, -2.828),
    float2(-3.696, -1.531),
    float2(-4.000, 0.000),
    float2(-3.696, 1.531),
    float2(-2.828, 2.828),
    float2(-1.531, 3.696),
    float2(0.000, 6.000),
    float2(1.553, 5.796),
    float2(3.000, 5.196),
    float2(4.243, 4.243),
    float2(5.196, 3.000),
    float2(5.796, 1.553),
    float2(6.000, 0.000),
    float2(5.796, -1.553),
    float2(5.196, -3.000),
    float2(4.243, -4.243),
    float2(3.000, -5.196),
    float2(1.553, -5.796),
    float2(0.000, -6.000),
    float2(-1.553, -5.796),
    float2(-3.000, -5.196),
    float2(-4.243, -4.243),
    float2(-5.196, -3.000),
    float2(-5.796, -1.553),
    float2(-6.000, -0.000),
    float2(-5.796, 1.553),
    float2(-5.196, 3.000),
    float2(-4.243, 4.243),
    float2(-3.000, 5.196),
    float2(-1.553, 5.796)
};


static const float dofScale = 1.5f;


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


float3 ComputeNear(float2 uv, float2 pixelSize)
{
    float3 result = 0.f;
    
    for (int sampleIdx = 0; sampleIdx < DOF_SAMPLES_NUM; ++sampleIdx)
    {
        const float2 sampleUV = uv + dofSamples[sampleIdx] * pixelSize * dofScale;
        result += PARAMS_D_O_COMPUTATION_PASS->linearColorTexture.SampleLevel(BindlessSamplers::LinearClampEdge(), sampleUV, 0.f);
    }

    return result / DOF_SAMPLES_NUM;
}


float3 ComputeFar(float2 uv, float2 pixelSize)
{
    float3 result = 0.f;
    float weight = 0.f;
    
    for (int sampleIdx = 0; sampleIdx < DOF_SAMPLES_NUM; ++sampleIdx)
    {
        const float2 sampleUV = uv + dofSamples[sampleIdx] * pixelSize * dofScale;
        result += PARAMS_D_O_COMPUTATION_PASS->linearColorMulFarTexture.SampleLevel(BindlessSamplers::LinearClampEdge(), sampleUV, 0.f);
        weight += PARAMS_D_O_COMPUTATION_PASS->cocTexture.SampleLevel(BindlessSamplers::LinearClampEdge(), sampleUV, 0.f).y;
    }

    return result / weight;
}


[numthreads(8, 8, 1)]
void DOFComputationCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;
    
    uint2 outputRes = PARAMS_D_O_COMPUTATION_PASS->farFieldDOFTexture.GetResolution();

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 pixelSize = rcp(float2(outputRes));
        
        const float2 uv = float2(pixel + 0.5f) * pixelSize;

        const float cocNear = PARAMS_D_O_COMPUTATION_PASS->cocNearBlurredTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv, 0.f);
        const float cocFar = PARAMS_D_O_COMPUTATION_PASS->cocTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv, 0.f).y;

        const float3 linearColor = PARAMS_D_O_COMPUTATION_PASS->linearColorTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv, 0.f);

        PARAMS_D_O_COMPUTATION_PASS->nearFieldDOFTexture[pixel] = cocNear > 0.f ? ComputeNear(uv, pixelSize) : linearColor;
        PARAMS_D_O_COMPUTATION_PASS->farFieldDOFTexture[pixel] = cocFar > 0.f ? ComputeFar(uv, pixelSize) : 0.f;
    }
}
