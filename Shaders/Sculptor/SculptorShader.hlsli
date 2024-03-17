#include "Hashing.hlsli"
#include "DebugFeatures.hlsli"

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

float3 RGBToYCoCg(float3 rgb)
{
    const float Y = rgb.r * 0.25f + rgb.g * 0.5f + rgb.b * 0.25f;
    const float Co = rgb.r * 0.5f + rgb.b * -0.5f;
    const float Cg = rgb.r * -0.25f + rgb.g * 0.5f + rgb.b * -0.25f;
    return float3(Y, Co, Cg);
}

float3 YCoCgToRGB(float3 ycocg)
{
    const float r = ycocg.x + ycocg.y - ycocg.z;
    const float g = ycocg.x + ycocg.z;
    const float b = ycocg.x - ycocg.y - ycocg.z;
    return float3(r, g, b);
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


float MaxComponent(in float3 value)
{
    return max(value.x, max(value.y, value.z));
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


// Based on: Chapter 16.6.1 of "Ray Tracing Gems"
float3 RandomVectorInCosineWeightedHemisphere(in float3x3 tangent, in float2 random, out float pdf)
{
    const float a2 = random.x;
    const float a = sqrt(a2);
    const float phi = 2.f * PI * random.y;
    float sinPhi, cosPhi;
    sincos(phi, OUT sinPhi, OUT cosPhi);

    const float x = a * cosPhi;
    const float y = a * sinPhi;
    const float z = sqrt(1.f - a2);

    const float3 direction = mul(tangent, float3(x, y, z));
    pdf = z/ PI;

    return direction;
}


// Based on: Chapter 16.6.2 of "Ray Tracing Gems"
float3 RandomVectorInCosineWeightedHemisphere(in float3 direction, in float2 random, out float pdf)
{
    const float a = 1.f - 2.f * random.x;
    const float b = sqrt(1.f - Pow2(a));
    const float phi = 2.f * PI * random.y;

    float sinPhi, cosPhi;
    sincos(phi, OUT sinPhi, OUT cosPhi);

    const float x = direction.x + b * cosPhi;
    const float y = direction.y + b * sinPhi;
    const float z = direction.z + a;

    pdf = a / PI;

    return float3(x, y, z);
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


float2 OctahedronEncode(in float3 direction)
{
    const float l1norm = abs(direction.x) + abs(direction.y) + abs(direction.z);
    float2 uv = direction.xy * (1.f / l1norm);
    if (direction.z < 0.f)
    {
        uv = (1.f - abs(uv.yx)) * SignNotZero(uv.xy);
    }
    return uv * 0.5f + 0.5f;
}

 
float3 OctahedronDecode(in float2 coords)
{
    coords = coords * 2.f - 1.f;

    float3 direction = float3(coords.x, coords.y, 1.f - abs(coords.x) - abs(coords.y));
    if (direction.z < 0.f)
    {
        direction.xy = (1.f - abs(direction.yx)) * SignNotZero(direction.xy);
    }
    return normalize(direction);
}

// Packing

uint PackFloat4x8(float4 value)
{
    const uint4 asUints = value * 255.f;
    return (asUints.r << 24) | (asUints.g << 16) | (asUints.b << 8) | asUints.a;
}


float4 UnpackFloat4x8(uint value)
{
    const uint4 asUints = uint4((value >> 24) & 0xFF, (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
    return asUints / 255.f;
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
