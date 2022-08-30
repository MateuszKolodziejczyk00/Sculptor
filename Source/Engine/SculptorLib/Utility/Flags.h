#pragma once

#include "MathTypes.h"

#define BIT(nth) (1 << (nth))
#define BIT64(nth) (static_cast<Flags64>(1) << (nth))

namespace spt::lib
{

template<typename TFlagsType, typename TFlagType,
		 typename = std::enable_if_t<std::is_integral_v<TFlagsType>, void>,
		 typename = std::enable_if_t<std::is_convertible_v<TFlagType, TFlagsType>, void>>
constexpr Bool HasAnyFlag(TFlagsType flags, TFlagType queriedFlag)
{
	return (flags & static_cast<TFlagsType>(queriedFlag)) != 0;
}


template<typename TFlagsType, typename TFlagType,
		 typename = std::enable_if_t<std::is_integral_v<TFlagsType>, void>,
		 typename = std::enable_if_t<std::is_convertible_v<TFlagType, TFlagsType>, void>>
constexpr Bool HasAllFlags(TFlagsType flags, TFlagType queriedFlag)
{
	const TFlagsType allFlags = static_cast<TFlagsType>(queriedFlag);
	return (flags & allFlags) == allFlags;
}


template<typename TFlagsType, typename TFlagType,
		 typename = std::enable_if_t<std::is_integral_v<TFlagsType>, void>,
		 typename = std::enable_if_t<std::is_convertible_v<TFlagType, TFlagsType>, void>>
void AddFlag(TFlagsType& flags, TFlagType flagToAdd)
{
	flags |= static_cast<TFlagsType>(flagToAdd);
}


template<typename TFlagsType, typename TFlagType,
		 typename = std::enable_if_t<std::is_integral_v<TFlagsType>, void>,
		 typename = std::enable_if_t<std::is_convertible_v<TFlagType, TFlagsType>, void>>
void RemoveFlag(TFlagsType& flags, TFlagType flagToRemove)
{
	flags &= ~static_cast<TFlagsType>(flagToRemove);
}

template<typename TEnumType, typename... TEnums,
		 typename = std::enable_if_t<std::is_enum_v<TEnumType>, void>>
constexpr TEnumType Union(TEnumType flag, TEnums... flags)
{
	static_assert((std::is_same_v<TEnumType, decltype(flags)> || ...));

	using UnderlyingType = std::underlying_type_t<TEnumType>;
	const UnderlyingType result = static_cast<UnderlyingType>(flag) | (static_cast<UnderlyingType>(flags) | ...);
	return static_cast<TEnumType>(result);
}


template<typename TEnumType, typename... TEnums,
		 typename = std::enable_if_t<std::is_enum_v<TEnumType>, void>>
constexpr TEnumType Difference(TEnumType flag, TEnums... flags)
{
	static_assert((std::is_same_v<TEnumType, decltype(flags)> || ...));

	using UnderlyingType = std::underlying_type_t<TEnumType>;
	const UnderlyingType result = static_cast<UnderlyingType>(flag) & ~(static_cast<UnderlyingType>(flags) | ...);
	return static_cast<TEnumType>(result);
}


template<typename TEnumType, typename... TEnums,
		 typename = std::enable_if_t<std::is_enum_v<TEnumType>, void>>
constexpr TEnumType Intersection(TEnumType flag, TEnums... flags)
{
	static_assert((std::is_same_v<TEnumType, decltype(flags)> || ...));

	using UnderlyingType = std::underlying_type_t<TEnumType>;
	const UnderlyingType result = static_cast<UnderlyingType>(flag) & (static_cast<UnderlyingType>(flags) & ...);
	return static_cast<TEnumType>(result);
}


template<typename TEnumType, typename... TEnums,
		 typename = std::enable_if_t<std::is_enum_v<TEnumType>, void>>
constexpr TEnumType Flags(TEnumType flag, TEnums... flags)
{
	return Union(flag, flags...);
}

template<typename TEnumType,
		 typename = std::enable_if_t<std::is_enum_v<TEnumType>, void>>
constexpr Bool HasAnyFlag(TEnumType flags, TEnumType queriedFlag)
{
	using UnderlyingType = std::underlying_type_t<TEnumType>;
	return (static_cast<UnderlyingType>(flags) & static_cast<UnderlyingType>(queriedFlag)) != 0;
}


template<typename TEnumType,
		 typename = std::enable_if_t<std::is_enum_v<TEnumType>, void>>
constexpr Bool HasAllFlags(TEnumType flags, TEnumType queriedFlag)
{
	using UnderlyingType = std::underlying_type_t<TEnumType>;
	return (static_cast<UnderlyingType>(flags) & static_cast<UnderlyingType>(queriedFlag)) == static_cast<UnderlyingType>(queriedFlag);
}


template<typename TEnumType,
		 typename isEnum = std::enable_if_t<std::is_enum_v<TEnumType>, void>>
void AddFlag(TEnumType& flags, TEnumType flagToAdd)
{
	flags = Union(flags, flagToAdd);
}


template<typename TEnumType,
		 typename = std::enable_if_t<std::is_enum_v<TEnumType>, void>>
void RemoveFlag(TEnumType& flags, TEnumType flagToRemove)
{
	flags = Difference(flags, flagToRemove);
}

} // spt::lib