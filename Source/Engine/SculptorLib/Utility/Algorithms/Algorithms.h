#pragma once

#include "Utility/Concepts.h"
#include "Remove.h"


namespace spt::lib
{

template<typename TContainer>
auto Contains(TContainer& container, const typename TContainer::value_type& val)
{
	return std::find(std::begin(container), std::end(container), val) != std::end(container);
}

template<typename TContainer, CPredicate<typename TContainer::reference> TPredicate>
auto ContainsPred(TContainer& container, const TPredicate& predicate)
{
	return std::find_if(std::begin(container), std::end(container), predicate) != std::end(container);
}

} // spt::lib
