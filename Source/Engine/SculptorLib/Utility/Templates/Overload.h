#pragma once

namespace spt::lib
{

// Overload pattern
// Source: https://www.cppstories.com/2018/06/variant/#overload

template<typename... TTypes>
struct Overload : TTypes...
{
	using TTypes::operator()...;
};

// template deduction rule
template<typename... TTypes>
Overload(TTypes...)->Overload<TTypes...>;

} // spt::lib