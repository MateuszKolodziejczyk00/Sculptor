#pragma once

#include <unordered_map>


namespace spt::lib
{


template <class TKeyType, class TValueType, class THasher = std::hash<TKeyType>, class TKeyEq = std::equal_to<TKeyType>,
    class TAllocator = std::allocator<std::pair<const TKeyType, TValueType>>>
class HashMap : public std::unordered_map<TKeyType, TValueType, THasher, TKeyEq, TAllocator>
{
protected:

	using Super = std::unordered_map<TKeyType, TValueType, THasher, TKeyEq, TAllocator>;

public:

	// All constructors from base class
	using Super::Super;
};

}