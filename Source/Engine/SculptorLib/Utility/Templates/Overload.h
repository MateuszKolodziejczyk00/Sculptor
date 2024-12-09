#pragma once

#include <variant>


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


template<typename TVariant, typename... TCallables>
auto CallOverload(TVariant&& variant, TCallables&&... callables)
{
	return std::visit(Overload{ std::forward<TCallables>(callables)... }, std::forward<TVariant>(variant));
}

} // spt::lib