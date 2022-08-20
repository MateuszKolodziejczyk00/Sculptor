#pragma once

#include <type_traits>

namespace spt::lib
{

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

}