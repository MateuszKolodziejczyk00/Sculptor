#pragma once

#include <unordered_set>


namespace spt::lib
{

template <class TKeyType, class THasher = std::hash<TKeyType>, class TKeyEq = std::equal_to<TKeyType>, class TAllocator = std::allocator<TKeyType>>
using HashSet = std::unordered_set<TKeyType, THasher, TKeyEq, TAllocator>;

}