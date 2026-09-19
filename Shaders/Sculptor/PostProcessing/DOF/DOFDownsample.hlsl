#include "SculptorShader.hlsli"

[[shader_params(DOFDownsampleParams, PARAMS_D_O_F_DOWNSAMPLE)]]

#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void DOFDownsampleCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;
    
    uint2 outputRes = PARAMS_D_O_F_DOWNSAMPLE->cocHalfTexture.GetResolution();

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 inputPixel = pixel * 2.f;
        const float2 uv00 = inputPixel / float2(PARAMS_D_O_F_DOWNSAMPLE->inputResolution);
        const float2 uv01 = inputPixel / float2(PARAMS_D_O_F_DOWNSAMPLE->inputResolution) + float2(0.f, PARAMS_D_O_F_DOWNSAMPLE->inputPixelSize.y);
        const float2 uv10 = inputPixel / float2(PARAMS_D_O_F_DOWNSAMPLE->inputResolution) + float2(PARAMS_D_O_F_DOWNSAMPLE->inputPixelSize.x, 0.f);
        const float2 uv11 = inputPixel / float2(PARAMS_D_O_F_DOWNSAMPLE->inputResolution) + PARAMS_D_O_F_DOWNSAMPLE->inputPixelSize;

        const float2 coc = PARAMS_D_O_F_DOWNSAMPLE->cocTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv00, 0).rg;

        const float cocFar00 = coc.y;
        const float cocFar01 = PARAMS_D_O_F_DOWNSAMPLE->cocTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv01, 0).y;
        const float cocFar10 = PARAMS_D_O_F_DOWNSAMPLE->cocTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv10, 0).y;
        const float cocFar11 = PARAMS_D_O_F_DOWNSAMPLE->cocTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv11, 0).y;
        
        const float weight00 = 1000.f;
        float3 colorMulCoCFar = weight00 * PARAMS_D_O_F_DOWNSAMPLE->linearColorTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv00, 0).rgb;
        float weightSum = weight00;

        const float weight01 = rcp(abs(cocFar00 - cocFar01) + 0.001f);
        colorMulCoCFar += weight01 * PARAMS_D_O_F_DOWNSAMPLE->linearColorTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv01, 0).rgb;
        weightSum += weight01;

        const float weight10 = rcp(abs(cocFar00 - cocFar10) + 0.001f);
        colorMulCoCFar += weight10 * PARAMS_D_O_F_DOWNSAMPLE->linearColorTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv10, 0).rgb;
        weightSum += weight10;

        const float weight11 = rcp(abs(cocFar00 - cocFar11) + 0.001f);
        colorMulCoCFar += weight11 * PARAMS_D_O_F_DOWNSAMPLE->linearColorTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv11, 0).rgb;
        weightSum += weight11;

        colorMulCoCFar /= weightSum;
        const float3 color = colorMulCoCFar;
        colorMulCoCFar *= coc.y;

        PARAMS_D_O_F_DOWNSAMPLE->cocHalfTexture[pixel]                 = coc;
	    PARAMS_D_O_F_DOWNSAMPLE->linearColorHalfTexture[pixel]         = color;
	    PARAMS_D_O_F_DOWNSAMPLE->linearColorMulFarHalfTexture[pixel]   = colorMulCoCFar;
    }
}
