#pragma once

#include "SculptorCoreTypes.h"
#include "RHI/RHICore/RHIShaderTypes.h"

namespace spt::sc
{

enum class EOptimizationType
{
	None,
	PreferFast
};

struct MacroDefinition
{
public:

	MacroDefinition()
	{ }

	MacroDefinition(lib::HashedString inMacro)
		: macro(inMacro)
	{ }

	MacroDefinition(lib::HashedString name, lib::HashedString value)
	{
		macro = lib::String(name.GetView()) + "=" + lib::String(value.GetView());
	}

	MacroDefinition(lib::HashedString name, const char* value)
		: MacroDefinition(name, lib::HashedString(value))
	{
	}

	MacroDefinition(lib::HashedString name, Bool value)
	{
		macro = lib::String(name.GetView()) + "=" + lib::String(value ? "1" : "0");
	}

	Bool IsValid() const
	{
		return macro.IsValid();
	}
	
	lib::HashedString		macro;
};

enum class ETargetEnvironment
{
	None,
	Vulkan_1_3
};

} // spt::sc