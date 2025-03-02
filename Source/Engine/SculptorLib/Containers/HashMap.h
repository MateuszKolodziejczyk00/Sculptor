#pragma once

#include <unordered_map>


namespace spt::lib
{

template <class TKeyType, class TValueType, class THasher = Hasher<TKeyType>, class TKeyEq = std::equal_to<TKeyType>,
    class TAllocator = std::allocator<std::pair<const TKeyType, TValueType>>>
using HashMap = std::unordered_map<TKeyType, TValueType, THasher, TKeyEq, TAllocator>;

}