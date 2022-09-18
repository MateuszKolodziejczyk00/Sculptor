#pragma once

#include "Utility/Concepts.h"


namespace spt::lib
{

template<typename TContainer, CPredicate<typename TContainer::reference> TPredicate>
auto RemoveAllBy(TContainer& container, const TPredicate& predicate)
{
	return container.erase(std::remove_if(std::begin(container), std::end(container), predicate), std::cend(container));
}

} // spt::lib