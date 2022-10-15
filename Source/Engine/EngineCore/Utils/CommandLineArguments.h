#pragma once

#include "EngineCoreMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::engn
{

class ENGINE_CORE_API CommandLineArguments
{
public:

	CommandLineArguments() = default;
	CommandLineArguments(Uint32 argsNum, char** arguments);

	void				Parse(Uint32 argsNum, char** arguments);

	Bool				Contains(lib::HashedString arg) const;
	lib::HashedString	GetValue(lib::HashedString arg) const;

private:

	lib::HashMap<lib::HashedString, lib::HashedString> m_args;
};

} // spt::engn