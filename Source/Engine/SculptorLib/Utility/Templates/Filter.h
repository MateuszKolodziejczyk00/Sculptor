#pragma once

#include "TypeTraits.h"


namespace spt::lib
{

template<template<typename TArg> typename TPredicate, typename... TArgs>
struct Filter
{ };

template<template<typename TArg> typename Predicate, typename TCurrentArg, typename... TArgs>
struct Filter<Predicate, TCurrentArg, TArgs...>
{
	using Type = std::conditional_t<Predicate<TCurrentArg>::value,
									typename TuplePushFront<TCurrentArg, typename Filter<Predicate, TArgs...>::Type>::Type,
									typename Filter<Predicate, TArgs...>::Type>;

};

template<template<typename TArg> typename Predicate>
struct Filter<Predicate>
{
	using Type = std::tuple<void>;
};

} // spt::lib
