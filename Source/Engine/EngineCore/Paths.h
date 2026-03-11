#pragma once

#include "EngineCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "FileSystem/File.h"


namespace spt::engn
{

class CommandLineArguments;


class ENGINE_CORE_API Paths
{
public:

	void Initialize(const CommandLineArguments& cmdLineArgs);

	lib::Path enginePath;
	lib::Path configsPath;
	lib::Path savedPath;
	lib::Path tracesPath;
	lib::Path gpuCrashDumpsPath;
	lib::Path contentPath;
	lib::Path executablePath;
	lib::Path executableDirectory;
	lib::String imguiConfigPath;
};

} // spt::engn
