#pragma once

#include "TypeTraits.h"


namespace spt::lib
{

template<template<typename TArg> typename TPredicate, typename... TArgs>
struct Filter
{ };

template<template<typename TArg> Bool Predicate, typename TCurrentArg, typename... TArgs>
struct Filter<Predicate, TCurrentArg, TArgs...>
{
	using Types = std::conditional_t<Predicate<TCurrentArg>,
									TuplePushFront<TCurrentArg, Filter<Predicate, TArgs...>::Type>::Type,
									Filter<Predicate, TArgs...>::Type...>;

};

template<template<typename TArg> Bool Predicate>
struct Filter<Predicate>
{
	using Types = std::tuple<void>;
};

} // spt::lib
