#pragma once

#include <array>


namespace spt::lib
{

template <class Type, SizeType Size>
class StaticArray : public std::array<Type, Size>
{
protected:

	using Super = std::array<Type, Size>;

public:

	// All constructors from base class
	using Super::Super;
};

}