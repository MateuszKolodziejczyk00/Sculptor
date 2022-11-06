#pragma once

#include <type_traits>

namespace spt::lib
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// AsParameter ===================================================================================

namespace details
{
template<typename TType> 
struct IsLargerThanRef
{
	static constexpr Bool value = sizeof(TType) > sizeof(TType&);
};

template<typename TType> 
struct AsParameter
{
	using Type = std::conditional_t<IsLargerThanRef<TType>::value, TType&, TType>;
};

template<typename TType> 
struct AsConstParameter
{
	using Type = std::conditional_t<IsLargerThanRef<TType>::value, const TType&, const TType>;
};

} // details

template<typename TType>
using AsParameter = details::AsParameter<TType>::Type;

template<typename TType>
using AsConstParameter = details::AsConstParameter<TType>::Type;

//////////////////////////////////////////////////////////////////////////////////////////////////
// IsPair ========================================================================================

template<typename TType>
struct IsPair : public std::false_type
{ };

template<typename TKey, typename TValue>
struct IsPair<std::pair<TKey, TValue>> : public std::true_type
{ };

template<typename TType>
static constexpr Bool isPair = IsPair<TType>::value;

static_assert(isPair<int> == false);
static_assert(isPair<std::pair<int, char>> == true);

//////////////////////////////////////////////////////////////////////////////////////////////////
// AllOf =========================================================================================

template<template<typename TTest> typename TPredicate, typename... TArgs>
struct AllOf;

template<template<typename TTest> typename TPredicate, typename TArg, typename... TArgs>
struct AllOf<TPredicate, TArg, TArgs...>
{
	static constexpr Bool value = TPredicate<TArg>::value && AllOf<TPredicate, TArgs...>::value;
};

template<template<typename TTest> typename TPredicate, typename TArg>
struct AllOf<TPredicate, TArg>
{
	static constexpr Bool value = TPredicate<TArg>::value;
};

template<template<typename TTest> typename TPredicate, typename... TArgs>
static constexpr Bool AllOf_v = AllOf<TPredicate, TArgs...>::value;

//////////////////////////////////////////////////////////////////////////////////////////////////
// AnyOf =========================================================================================

template<template<typename TTest> typename TPredicate, typename... TArgs>
struct AnyOf;

template<template<typename TTest> typename TPredicate, typename TArg, typename... TArgs>
struct AnyOf<TPredicate, TArg, TArgs...>
{
	static constexpr Bool value = TPredicate<TArg>::value || AnyOf<TPredicate, TArgs...>::value;
};

template<template<typename TTest> typename TPredicate, typename TArg>
struct AnyOf<TPredicate, TArg>
{
	static constexpr Bool value = TPredicate<TArg>::value;
};

template<template<typename TTest> typename TPredicate, typename... TArgs>
static constexpr Bool AnyOf_v = AnyOf<TPredicate, TArgs...>::value;

//////////////////////////////////////////////////////////////////////////////////////////////////
// TuplePushFront ================================================================================

template<typename, typename>
struct TuplePushFront
{ };

template<typename TType, typename... TArgs>
struct TuplePushFront<TType, std::tuple<TArgs...>>
{
	using Type = std::tuple<TType, TArgs...>;
};

template<typename TType>
struct TuplePushFront<TType, std::tuple<void>>
{
	using Type = std::tuple<TType>;
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// ParameterPackSize =============================================================================

template<typename... Ts>
struct ParameterPackSize
{
	static constexpr SizeType Count = sizeof...(Ts);
};

} // spt::lib