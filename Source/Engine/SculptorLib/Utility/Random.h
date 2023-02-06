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

}; // rnd

} // spt::lib