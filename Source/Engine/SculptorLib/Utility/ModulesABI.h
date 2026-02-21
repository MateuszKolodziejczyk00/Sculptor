#pragma once

#include "SculptorAliases.h"
#include "Utility/TypeID.h"


namespace spt::lib
{

struct ModuleExportDescriptor
{
	Uint32 apiOffset = 0u;
	const char* name = nullptr;
};



struct ModuleABI
{
	TypeID apiType;

	lib::Span<const ModuleExportDescriptor> exports;
};

} // spt::lib
