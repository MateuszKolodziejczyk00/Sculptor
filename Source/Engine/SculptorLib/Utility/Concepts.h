#pragma once

namespace spt::lib
{

template<typename TType, typename TPredicateArg>
concept CPredicate = requires(TType val)
{
	{ val(std::declval<TPredicateArg>()) } -> std::same_as<bool>;
};

} // spt::lib