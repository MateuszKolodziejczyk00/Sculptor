#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructsRegistry.h"


namespace spt::sc
{

class ShaderStructRegistration
{
public:

	ShaderStructRegistration(lib::HashedString structName, lib::String structSourceCode)
	{
		ShaderStructsRegistry::RegisterStructDefinition(structName, ShaderStructDefinition(std::move(structSourceCode)));
	}
};

} // spt::sc