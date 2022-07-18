#pragma once

#include <vector>


namespace spt::lib
{

template <class Type, class Allocator = std::allocator<Type>>
class DynamicArray : public std::vector<Type, Allocator>
{
protected:

	using Super = std::vector<Type, Allocator>;

public:

	// All constructors from base class
	using Super::Super;
};

}