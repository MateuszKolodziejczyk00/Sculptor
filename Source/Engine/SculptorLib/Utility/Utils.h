#pragma once

#include "MathTypes.h"

#define BIT(nth) (1 << (nth))
#define BIT64(nth) (static_cast<Flags64>(1) << (nth))


namespace spt::lib
{

template<typename FlagsType, typename FlagType>
std::enable_if_t<std::is_enum_v<FlagType>, Bool> HasFlag(FlagsType flags, FlagType queriedFlag)
{
	return (flags & static_cast<FlagsType>(queriedFlag)) != 0;
}

template<typename FlagsType, typename FlagType>
std::enable_if_t<std::is_enum_v<FlagType>, void> AddFlag(FlagsType& flags, FlagType flagToAdd)
{
	flags |= static_cast<FlagsType>(flagToAdd);
}

template<typename FlagsType, typename FlagType>
std::enable_if_t<std::is_enum_v<FlagType>, void> RemoveFlag(FlagsType& flags, FlagType flagToAdd)
{
	flags &= ~static_cast<FlagsType>(flagToAdd);
}

} // spt::lib