#pragma once

#include "SculptorCoreTypes.h"

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

	MacroDefinition(lib::HashedString macro)
		: m_macro(macro)
	{ }

	MacroDefinition(lib::HashedString name, lib::HashedString value)
	{
		SPT_PROFILE_FUNCTION();
	
		m_macro = lib::String(name.GetView()) + " " + lib::String(value.GetView());
	}

	Bool IsValid() const
	{
		return m_macro.IsValid();
	}
	
	lib::HashedString		m_macro;
};

enum class ETargetEnvironment
{
	None,
	Vulkan_1_3
};

enum class ECompiledShaderType
{
	None,
	Vertex,
	Fragment,
	Compute
};

}