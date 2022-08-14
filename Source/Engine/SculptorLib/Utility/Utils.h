#pragma once

#include "MathTypes.h"

#define BIT(nth) (1 << (nth))
#define BIT64(nth) (static_cast<Flags64>(1) << (nth))


namespace spt::lib
{

template<typename FlagsType>
Bool HasFlag(FlagsType flags, FlagsType queriedFlag)
{
	static_assert(std::is_integral_v<FlagsType>, "FlagsType must be integral");

	return (flags & queriedFlag) != 0;
}

template<typename FlagsType>
void AddFlag(FlagsType& flags, FlagsType flagToAdd)
{
	static_assert(std::is_integral_v<FlagsType>, "FlagsType must be integral");
	flags |= flagToAdd;
}

template<typename FlagsType>
void RemoveFlag(FlagsType& flags, FlagsType flagToAdd)
{
	static_assert(std::is_integral_v<FlagsType>, "FlagsType must be integral");
	flags &= ~flagToAdd;
}

} // spt::lib