#include "Hashing.hlsli"

#define SPT_SINGLE_ARG(...) __VA_ARGS__

#define IDX_NONE_32 0xffffffff

#define PI 3.14159265359
#define INV_PI 0.31830988618

#define TWO_PI 6.28318530718
#define INV_TWO_PI 0.15915494309


#define SMALL_NUMBER 1.e-6f


#define IN
#define OUT
#define INOUT

#define ZERO_VECTOR     (float3(0.f, 0.f, 0.f))

#define FORWARD_VECTOR  (float3(1.f, 0.f, 0.f))
#define RIGHT_VECTOR    (float3(0.f, 1.f, 0.f))
#define UP_VECTOR       (float3(0.f, 0.f, 1.f))


#if defined(SPT_META_PARAM_DEBUG_FEATURES)

#define SPT_ENABLE_CHECKS true

#include "Debug/DebugMessage.hlsli"

#else

#define SPT_DEBUG_PRINTF(...)
#define SPT_CHECK_MSG(...)
#define SPT_ENABLE_CHECKS false

#endif // defined(SPT_META_PARAM_DEBUG_FEATURES)


struct TextureCoord
{
	float2 uv;
	float2 duv_dx;
	float2 duv_dy;
};


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

bool IsNearlyZero(in float value, in float epsilon = 0.000001f)
{
	return abs(value) < epsilon;
}

bool AreNearlyEqual(in float a, in float b, in float epsilon = 0.000001f)
{
	return abs(a - b) < epsilon;
}

float Luminance(float3 color)
{
    return dot(color, float3(0.2126729, 0.7151522, 0.0721750));
}


float MaxComponent(in float3 value)
{
    return max(value.x, max(value.y, value.z));
}


float S_Curve(float x, float steepness)
{
    const float val = x * 2.f - 1.f;
    return saturate((atan(val * steepness) / atan(steepness)) * 0.5f + 0.5f);
}


float3 FibbonaciSphereDistribution(in int i, in int num)
{
    const float b = (sqrt(5.f) * 0.5f + 0.5f) - 1.f;
    float phi = 2.f * PI * frac(i * b);
    float cosTheta = 1.f - (2.f * i + 1.f) * (1.f / num);
    float sinTheta = sqrt(saturate(1.f - (cosTheta * cosTheta)));

    return float3((cos(phi) * sinTheta), (sin(phi) * sinTheta), cosTheta);
}

float RadicalInverse_VdC(in uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(in uint i, in uint N)
{
    return float2(float(i)/float(N), RadicalInverse_VdC(i));
}  

// Octahedron mapping =========================================================================

float SignNotZero(float v)
{
    return (v >= 0.f) ? 1.f : -1.f;
}


float2 SignNotZero(float2 v)
{
    return float2(SignNotZero(v.x), SignNotZero(v.y));
}

// Complex numbers

float2 ComplexMultiply(in float2 rhs, in float2 lhs)
{
    return float2(rhs.x * lhs.x - rhs.y * lhs.y, rhs.x * lhs.y + rhs.y * lhs.x);
}


float GaussianBlurWeight(int x, float sigma)
{
    const float sigmaSquared = Pow2(sigma);
    return exp(-0.5f * Pow2(x) / sigmaSquared) / (sqrt(2.f * PI * sigmaSquared));
}
