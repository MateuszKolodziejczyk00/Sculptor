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