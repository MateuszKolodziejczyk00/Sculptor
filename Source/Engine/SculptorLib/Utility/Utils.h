#pragma once

#include "MathTypes.h"

#define BIT(nth) (1 << (nth))
#define BIT64(nth) (static_cast<Flags64>(1) << (nth))


namespace spt::lib
{

template<typename FlagsType, typename FlagType,
		 typename = std::enable_if_t<std::is_integral_v<FlagsType>, void>,
		 typename = std::enable_if_t<std::is_enum_v<FlagType>, void>>
constexpr Bool HasAnyFlag(FlagsType flags, FlagType queriedFlag)
{
	return (flags & static_cast<FlagsType>(queriedFlag)) != 0;
}


template<typename FlagsType, typename FlagType,
		 typename = std::enable_if_t<std::is_integral_v<FlagsType>, void>,
		 typename = std::enable_if_t<std::is_enum_v<FlagType>, void>>
constexpr Bool HasAllFlags(FlagsType flags, FlagType queriedFlag)
{
	const FlagsType allFlags = static_cast<FlagsType>(queriedFlag);
	return (flags & allFlags) == allFlags;
}


template<typename FlagsType, typename FlagType,
		 typename = std::enable_if_t<std::is_integral_v<FlagsType>, void>,
		 typename = std::enable_if_t<std::is_enum_v<FlagType>, void>>
void AddFlag(FlagsType& flags, FlagType flagToAdd)
{
	flags |= static_cast<FlagsType>(flagToAdd);
}


template<typename FlagsType, typename FlagType,
		 typename = std::enable_if_t<std::is_integral_v<FlagsType>, void>,
		 typename = std::enable_if_t<std::is_enum_v<FlagType>, void>>
void RemoveFlag(FlagsType& flags, FlagType flagToRemove)
{
	flags &= ~static_cast<FlagsType>(flagToRemove);
}


template<typename EnumType, typename... Enums,
		 typename = std::enable_if_t<std::is_enum_v<EnumType>, void>>
constexpr EnumType Union(EnumType flag, Enums... flags)
{
	static_assert((std::is_same_v<EnumType, decltype(flags)> || ...));

	using UnderlyingType = std::underlying_type_t<EnumType>;
	const UnderlyingType result = static_cast<UnderlyingType>(flag) | (static_cast<UnderlyingType>(flags) | ...);
	return static_cast<EnumType>(result);
}


template<typename EnumType, typename... Enums,
		 typename = std::enable_if_t<std::is_enum_v<EnumType>, void>>
constexpr EnumType Difference(EnumType flag, Enums... flags)
{
	static_assert((std::is_same_v<EnumType, decltype(flags)> || ...));

	using UnderlyingType = std::underlying_type_t<EnumType>;
	const UnderlyingType result = static_cast<UnderlyingType>(flag) & ~(static_cast<UnderlyingType>(flags) | ...);
	return static_cast<EnumType>(result);
}


template<typename EnumType, typename... Enums,
		 typename = std::enable_if_t<std::is_enum_v<EnumType>, void>>
constexpr EnumType Intersection(EnumType flag, Enums... flags)
{
	static_assert((std::is_same_v<EnumType, decltype(flags)> || ...));

	using UnderlyingType = std::underlying_type_t<EnumType>;
	const UnderlyingType result = static_cast<UnderlyingType>(flag) & (static_cast<UnderlyingType>(flags) & ...);
	return static_cast<EnumType>(result);
}


template<typename EnumType, typename... Enums,
		 typename = std::enable_if_t<std::is_enum_v<EnumType>, void>>
constexpr EnumType Flags(EnumType flag, Enums... flags)
{
	return Union(flag, flags);
}

template<typename EnumType,
		 typename = std::enable_if_t<std::is_enum_v<EnumType>, void>>
constexpr Bool HasAnyFlag(EnumType flags, EnumType queriedFlag)
{
	using UnderlyingType = std::underlying_type_t<EnumType>;
	return (static_cast<UnderlyingType>(flags) & static_cast<UnderlyingType>(queriedFlag)) != 0;
}


template<typename EnumType,
		 typename = std::enable_if_t<std::is_enum_v<EnumType>, void>>
constexpr Bool HasAllFlags(EnumType flags, EnumType queriedFlag)
{
	using UnderlyingType = std::underlying_type_t<EnumType>;
	return (static_cast<UnderlyingType>(flags) & static_cast<UnderlyingType>(queriedFlag)) == static_cast<UnderlyingType>(queriedFlag);
}


template<typename EnumType,
		 typename = std::enable_if_t<std::is_enum_v<EnumType>, void>>
void AddFlag(EnumType& flags, EnumType flagToAdd)
{
	flags = Union(flags, flagToAdd);
}


template<typename EnumType,
		 typename = std::enable_if_t<std::is_enum_v<EnumType>, void>>
void RemoveFlag(EnumType& flags, EnumType flagToRemove)
{
	flags = Difference(flags, flagToRemove);
}

} // spt::lib