#include "SculptorShader.hlsli"

[[descriptor_set(TemporalAADS, 0)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
    uint3 groupID : SV_GroupID;
    uint3 localID : SV_GroupThreadID;
};


#define GROUP_SIZE 8

#define SHARED_DATA_SIZE (GROUP_SIZE + 2)

groupshared float sharedDepths[SHARED_DATA_SIZE][SHARED_DATA_SIZE];
groupshared float3 sharedColors[SHARED_DATA_SIZE][SHARED_DATA_SIZE];


void CacheGroupLocalDepths(CS_INPUT input, float2 pixelSize, int2 outputRes)
{
    for (uint i = input.localID.x + input.localID.y * GROUP_SIZE; i < SHARED_DATA_SIZE * SHARED_DATA_SIZE; i += GROUP_SIZE * GROUP_SIZE)
    {
        const uint x = i / SHARED_DATA_SIZE;
        const uint y = i - (x * SHARED_DATA_SIZE);

        int2 pixel = (input.groupID.xy * 8) - int2(1, 1) + int2(x, y);
        pixel.x = clamp(pixel.x, 0, outputRes.x - 1);
        pixel.y = clamp(pixel.y, 0, outputRes.y - 1);

        const float depth = u_depth.SampleLevel(u_nearestSampler, pixel * pixelSize, 0);
        sharedDepths[x][y] = depth;

        const float3 color = u_outputColor[pixel];
        sharedColors[x][y] = color;
    }
}


int2 FindClosestPixelOffset3x3(uint2 localID)
{
    float closestDepth = 0.f; // we use inverse depth
    int2 closestDepthPixelOffset = 0;
    for (int i = -1; i <= 1; ++i)
    {
        for (int j = -1; j <= 1; ++j)
        {
            const float neighbourDepth = sharedDepths[localID.x + i + 1][localID.y + j + 1];
            if(neighbourDepth > closestDepth)
            {
                closestDepth = neighbourDepth;
                closestDepthPixelOffset = int2(i, j);
            }
        }
    }

    return closestDepthPixelOffset;
}


// Based on: https://www.shadertoy.com/view/MtVGWz
float3 SampleCatmullRom(Texture2D<float3> texture, SamplerState textureSampler, float2 uv, float2 resolution)
{
    // We're going to sample a a 4x4 grid of texels surrounding the target UV coordinate. We'll do this by rounding
    // down the sample location to get the exact center of our "starting" texel. The starting texel will be at
    // location [1, 1] in the grid, where [0, 0] is the top left corner.
    const float2 samplePosition = uv * resolution;
    const float2 texPos1 = floor(samplePosition - 0.5f) + 0.5f;

    // Compute the fractional offset from our starting texel to our original sample location, which we'll
    // feed into the Catmull-Rom spline function to get our filter weights.
    const float2 f = samplePosition - texPos1;

    // Compute the Catmull-Rom weights using the fractional offset that we calculated earlier.
    // These equations are pre-expanded based on our knowledge of where the texels will be located,
    // which lets us avoid having to evaluate a piece-wise function.
    const float2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
    const float2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
    const float2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
    const float2 w3 = f * f * (-0.5f + 0.5f * f);

    // Work out weighting factors and sampling offsets that will let us use bilinear filtering to
    // simultaneously evaluate the middle 2 samples from the 4x4 grid.
    const float2 w12 = w1 + w2;
    const float2 offset_12 = w2 / (w1 + w2);

    // Compute the final UV coordinates we'll use for sampling the texture
    float2 texPos0 = texPos1 - 1;
    float2 texPos3 = texPos1 + 2;
    float2 texPos12 = texPos1 + offset_12;

    texPos0 /= resolution;
    texPos3 /= resolution;
    texPos12 /= resolution;

    float3 result = 0.f;
    
    result += texture.SampleLevel(textureSampler, float2(texPos0.x, texPos0.y), 0).rgb * w0.x * w0.y;
    result += texture.SampleLevel(textureSampler, float2(texPos12.x, texPos0.y), 0).rgb * w12.x * w0.y;
    result += texture.SampleLevel(textureSampler, float2(texPos3.x, texPos0.y), 0).rgb * w3.x * w0.y;
    
    result += texture.SampleLevel(textureSampler, float2(texPos0.x, texPos12.y), 0).rgb * w0.x * w12.y;
    result += texture.SampleLevel(textureSampler, float2(texPos12.x, texPos12.y), 0).rgb * w12.x * w12.y;
    result += texture.SampleLevel(textureSampler, float2(texPos3.x, texPos12.y), 0).rgb * w3.x * w12.y;

    result += texture.SampleLevel(textureSampler, float2(texPos0.x, texPos3.y), 0).rgb * w0.x * w3.y;
    result += texture.SampleLevel(textureSampler, float2(texPos12.x, texPos3.y), 0).rgb * w12.x * w3.y;
    result += texture.SampleLevel(textureSampler, float2(texPos3.x, texPos3.y), 0).rgb * w3.x * w3.y;

    return result;
}


// Based on https://github.com/TheRealMJP/MSAAFilter/blob/master/MSAAFilter/Resolve.hlsl
float FilterCubic(float x, float B, float C)
{
    float y = 0.0f;
    float x2 = x * x;
    float x3 = x * x * x;
    if(x < 1)
        y = (12 - 9 * B - 6 * C) * x3 + (-18 + 12 * B + 6 * C) * x2 + (6 - 2 * B);
    else if (x <= 2)
        y = (-B - 6 * C) * x3 + (6 * B + 30 * C) * x2 + (-12 * B - 48 * C) * x + (8 * B + 24 * C);

    return y / 6.0f;
}


float FilterMitchell(float value)
{
    const float cubicValue = value;
    const float ab = rcp(3.0f);
    return FilterCubic(cubicValue, ab, ab);
}


float SubsampleWeight(float distance)
{
    // Cubic filters works on [-2, 2] domain, thus scale the value by 2
    return FilterMitchell(distance * 2.f);
}


// Based on TAA in Inside game
float3 ClipAABB(float3 aabbMin, float3 aabbMax, float3 historySample)
{
    const float3 pClip = 0.5f * (aabbMax + aabbMin);
    const float3 eClip = 0.5f * (aabbMax - aabbMin) + 0.000000001f;

    const float3 vClip = historySample - pClip;
    const float3 vUnit = vClip.xyz / eClip;
    const float3 aUnit = abs(vUnit);
    const float maUnit = max(aUnit.x, max(aUnit.y, aUnit.z));

    if (maUnit > 1.0)
    {
        return (pClip) + vClip / maUnit;
    }
    else
    {
        // point inside aabb
        return historySample;
    }
}


[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void TemporalAACS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;

    uint2 outputRes;
    u_outputColor.GetDimensions(outputRes.x, outputRes.y);

    const float2 pixelSize = rcp(float2(outputRes));

    CacheGroupLocalDepths(input, pixelSize, outputRes);

    GroupMemoryBarrierWithGroupSync();
    
    const float2 uv = pixel * pixelSize + pixelSize * 0.5f;

    if (pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const int2 closestPixelOffset = FindClosestPixelOffset3x3(input.localID.xy);

        const float2 closestNeighborUV = uv + pixelSize * closestPixelOffset;
        
        const float2 motion = u_motion.SampleLevel(u_nearestSampler, closestNeighborUV, 0);

        const float2 reporojectedUV = uv - motion;

        if (all(reporojectedUV >= 0.f) && all(reporojectedUV <= 1.f))
        {
            float currentSampleWeight = 0.f;
            float3 currentSampleSum = 0.f;
            
            float3 neighborhoodMin = 9999.f;
            float3 neighborhoodMax = -9999.f;

            // Moments
            float3 m1 = 0.f;
            float3 m2 = 0.f;

            for(int y = -1; y <= 1; ++y)
            {
                for(int x = -1; x <= 1; ++x)
                {
                    const int2 offset = int2(x, y);
                    const float2 neighborUV = uv + pixelSize * offset;

                    float3 neighborColor = u_inputColor.SampleLevel(u_linearSampler, neighborUV, 0);

                    if(u_params.useYCoCg)
                    {
                        neighborColor = RGBToYCoCg(neighborColor);
                    }

                    const float subsampleDistance = length(float2(x, y));
                    
                    const float subsampleWeight = SubsampleWeight(subsampleDistance);

                    currentSampleSum += neighborColor * subsampleWeight;
                    currentSampleWeight += subsampleWeight;

                    neighborhoodMin = min(neighborhoodMin, neighborColor);
                    neighborhoodMax = max(neighborhoodMax, neighborColor);

                    m1 += neighborColor;
                    m2 += neighborColor * neighborColor;
                }
            }
            
            const float3 currentSample = currentSampleSum / currentSampleWeight;

            // Shrink chroma min-max
            if(u_params.useYCoCg)
            {
                const float chromaExtent = 0.125f * (neighborhoodMax.x - neighborhoodMin.x);
                const float2 chromaCenter = currentSample.yz;
                neighborhoodMax.yz = chromaCenter + chromaExtent;
                neighborhoodMin.yz = chromaCenter - chromaExtent;
            }

            float3 historySample = SampleCatmullRom(u_historyColor, u_linearSampler, reporojectedUV, float2(outputRes));

            if(u_params.useYCoCg)
            {
                historySample = RGBToYCoCg(historySample);
            }

            const float invSamplesNum = rcp(9.f);
            const float gamma = 1.f;
            const float3 mu = m1 * invSamplesNum;
            const float3 sigma = sqrt(abs((m2 * invSamplesNum) - (mu * mu)));
            const float3 minc = mu - gamma * sigma;
            const float3 maxc = mu + gamma * sigma;

            const float3 clampedhistorySample = clamp(historySample, neighborhoodMin, neighborhoodMax);
            historySample = ClipAABB(minc, maxc, clampedhistorySample);

            float3 historyWeight = 0.85f;
            float3 currentWeight = 1.f - historyWeight;

            const float currentLuminance = u_params.useYCoCg ? currentSample.r : Luminance(currentSample);
            const float historyLuminance = u_params.useYCoCg ? historySample.r : Luminance(historySample);
            
            historyWeight /= max(historyLuminance, 0.01f);
            currentWeight /= max(currentLuminance, 0.01f);

            historyWeight = max(historyWeight / (historyWeight + currentWeight), 0.85f);
            currentWeight = 1.f - historyWeight;
            
            float3 outputColor = (historyWeight * historySample + currentWeight * currentSample);

            if(u_params.useYCoCg)
            {
                outputColor = YCoCgToRGB(outputColor);
            }

            u_outputColor[pixel] = outputColor;
        }
        else
        {
            const float3 inputColor = u_inputColor.SampleLevel(u_nearestSampler, uv, 0);
            u_outputColor[pixel] = inputColor;
        }
    }
}
