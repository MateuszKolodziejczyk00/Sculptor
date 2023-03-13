#include "Hashing.hlsli"

#define IDX_NONE_32 0xffffffff

#define PI 3.14159265359
#define INV_PI 0.31830988618

#define TWO_PI 6.28318530718


template<typename TType>
TType Pow2(TType val)
{
    return val * val;
}

template<typename TType>
TType Pow3(TType val)
{
    return Pow2(val) * val;
}

template<typename TType>
TType Pow4(TType val)
{
    return Pow2(val) * Pow2(val);
}

template<typename TType>
TType Pow5(TType val)
{
    return Pow4(val) * val;
}

float Luminance(float3 color)
{
    return dot(color, float3(0.2126729, 0.7151522, 0.0721750));
}

// Source: "NEXT GENERATION POST PROCESSING IN CALL OF DUTY: ADVANCED WARFARE"
float InterleavedGradientNoise(float2 uv)
{
	const float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
	return frac(magic.z * frac(dot(uv, magic.xy)));
}


float2x2 NoiseRotation(float noise)
{
    float sin;
    float cos;
    sincos(2.f * PI * noise, sin, cos);

    return float2x2(cos, -sin,
                    sin, cos);

}

float2x2 InterleavedGradientNoiseRotation(float2 uv)
{
    return NoiseRotation(InterleavedGradientNoise(uv));
}


float Random(float2 seed)
{
    return frac(sin(dot(seed, float2(12.9898, 78.233))) * 43758.5453);
}


float S_Curve(float x, float steepness)
{
    const float val = x * 2.f - 1.f;
    return saturate((atan(val * steepness) / atan(steepness)) * 0.5f + 0.5f);
}

// Source: https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab
float3 LinearTosRGB(in float3 color)
{
    float3 x = color * 12.92f;
    float3 y = 1.055f * pow(saturate(color), 1.0f / 2.4f) - 0.055f;

    float3 clr = color;
    clr.r = color.r < 0.0031308f ? x.r : y.r;
    clr.g = color.g < 0.0031308f ? x.g : y.g;
    clr.b = color.b < 0.0031308f ? x.b : y.b;

    return clr;
}


float3 VectorInCone(float3 coneDir, float coneHalfAngleRad, float2 random)
{
    // Compute the basis vectors for the cone
    const float3 u = normalize(cross(coneDir, float3(0.f, 1.f, 0.f)));
    const float3 v = normalize(cross(coneDir, u));
    
    const float phi = 2.f * PI * random.y;

    const float minZ = cos(coneHalfAngleRad);
    const float z = lerp(minZ, 1.f, random.x);

    const float theta = acos(z);
    
    // Compute the random vector in spherical coordinates
    const float3 randomVector = sin(theta) * (cos(phi) * u + sin(phi) * v) + cos(theta) * coneDir;
    
    // Normalize the vector and return it
    return normalize(randomVector);
}
