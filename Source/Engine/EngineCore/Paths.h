#pragma once

#include "EngineCoreMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::engn
{

class CommandLineArguments;


class ENGINE_CORE_API Paths
{
public:

	static void					Initialize(const CommandLineArguments& cmdLineArgs);

	// Getters ===========================================

	static const lib::String&	GetEnginePath();

	static const lib::String&	GetConfigPath();

	// Utils =============================================

	static void AppendPath(lib::String& path, lib::StringView pathToAppend);

	static lib::String Combine(lib::StringView pathBegin, lib::StringView pathEnd);
};

} // spt::engn
