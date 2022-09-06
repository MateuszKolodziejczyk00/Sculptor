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
		SPT_PROFILER_FUNCTION();
	
		macro = lib::String(name.GetView()) + " " + lib::String(value.GetView());
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

}