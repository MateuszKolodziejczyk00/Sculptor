#pragma once

#include "SculptorAliases.h"
#include "Utility/TypeID.h"


namespace spt::lib
{

struct ModuleDescriptor
{
	const void* api = nullptr;
	lib::TypeID apiType;
};

} // spt::lib
