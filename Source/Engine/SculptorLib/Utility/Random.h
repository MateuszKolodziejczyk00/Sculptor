#pragma once

#include "SculptorAliases.h"
#include "MathCore.h"
#include <random>


namespace spt::lib
{

namespace rnd
{

inline auto& GetGenerator()
{
    using GeneratorType = std::mt19937;

    static std::random_device device;
    static GeneratorType generator(device());
    return generator;
}

template<std::floating_point TType>
inline TType Random()
{
    return std::generate_canonical<TType, sizeof(TType)>(GetGenerator());
}

template<std::floating_point TType>
inline TType Random(TType min, TType max)
{
    return min + Random<TType>() * (max - min);
}

template<std::integral TType>
inline TType Random(TType min, TType max)
{
    std::uniform_int_distribution<TType> distribution(min, max);
    return distribution(GetGenerator());
}

inline math::Matrix3f RandomRotationMatrix()
{
    // Based on James Arvo's algorithm from Graphics Gems 3

    // Setup a random rotation matrix using 3 uniform random values
    const Real32 u1 = math::Pi<Real32> * Random<Real32>(0.f, 1.f);
    const Real32 cos1 = cosf(u1);
    const Real32 sin1 = sinf(u1);

    const Real32 u2 = math::Pi<Real32> * Random<Real32>(0.f, 1.f);
    const Real32 cos2 = cosf(u2);
    const Real32 sin2 = sinf(u2);

    const Real32 u3 = Random<Real32>(0.f, 1.f);
    const Real32 sq3 = 2.f * sqrtf(u3 * (1.f - u3));

    const Real32 s2 = 2.f * u3 * sin2 * sin2 - 1.f;
    const Real32 c2 = 2.f * u3 * cos2 * cos2 - 1.f;
    const Real32 sc = 2.f * u3 * sin2 * cos2;

    // Create the random rotation matrix
    const Real32 _11 = cos1 * c2 - sin1 * sc;
    const Real32 _21 = sin1 * c2 + cos1 * sc;
    const Real32 _31 = sq3 * cos2;

    const Real32 _12 = cos1 * sc - sin1 * s2;
    const Real32 _22 = sin1 * sc + cos1 * s2;
    const Real32 _32 = sq3 * sin2;

    const Real32 _13 = cos1 * (sq3 * cos2) - sin1 * (sq3 * sin2);
    const Real32 _23 = sin1 * (sq3 * cos2) + cos1 * (sq3 * sin2);
    const Real32 _33 = 1.f - 2.f * u3;

    math::Matrix3f result;
    result.row(0) = math::Vector3f(_11, _12, _13);
    result.row(1) = math::Vector3f(_21, _22, _23);
    result.row(2) = math::Vector3f(_31, _32, _33);

    return result;
}

}; // rnd

} // spt::lib