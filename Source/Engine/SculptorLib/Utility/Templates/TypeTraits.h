#pragma once

#include <type_traits>

namespace spt::lib
{

/*
namespace details
{
template<typename TType> 
struct AsParameterBase
{
	static constexpr Bool isLargerThanRef = sizeof(TType) > sizeof(TType&);
};

template<typename TType> 
struct AsParameter : public AsParameterBase<TType>
{
	using Type = std::conditional_t<isLargerThanRef, TType&, TType>;
};

template<typename TType> 
struct AsConstParameter : public AsParameterBase<TType>
{
	using Type = std::conditional_t<isLargerThanRef, const TType&, const TType>;
};

} // details

template<typename TType>
using AsParameter = details::AsParameter<TType>::Type;

template<typename TType>
using AsConstParameter = details::AsConstParameter<TType>::Type;
*/


template<typename TType>
struct IsPair : public std::false_type
{ };

template<typename TKey, typename TValue>
struct IsPair<std::pair<TKey, TValue>> : public std::true_type
{ };

template<typename TType>
constexpr Bool isPair = IsPair<TType>::value;

static_assert(isPair<int> == false);
static_assert(isPair<std::pair<int, char>> == true);


/*
template<typename TPredicate, typename... TArgs>
struct AllOf
{
	static constexpr value = (TPredicate<TArgs>::value && ...);
};

template<typename TPredicate, typename... TArgs>
using AllOf_v = AllOf<TPredicate, TArgs...>::value;


template<typename TPredicate, typename... TArgs>
struct AnyOf
{
	static constexpr value = (TPredicate<TArgs>::value || ...);
};

template<typename TPredicate, typename... TArgs>
using AnyOf_v = AnyOf<TPredicate, TArgs...>::value;
*/

}