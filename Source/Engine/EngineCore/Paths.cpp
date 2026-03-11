#include "Paths.h"
#include "FileSystem/File.h"
#include "Utils/CommandLineArguments.h"


namespace spt::engn
{

namespace params
{

static const char* enginePath = "-EngineRelativePath";

} // params


void Paths::Initialize(const CommandLineArguments& cmdLineArgs)
{
	enginePath          = cmdLineArgs.GetValue(params::enginePath).ToString();
	configsPath         = enginePath / "Config";
	savedPath           = enginePath / "Saved";
	tracesPath          = enginePath / "Saved/Traces";
	gpuCrashDumpsPath   = enginePath / "Saved/GPUCrashDumps";
	contentPath         = enginePath / "Content";
	executablePath      = platf::Platform::GetExecutablePath();
	executableDirectory = executablePath.parent_path().string();
	imguiConfigPath     = (configsPath / "ImGuiConfig.ini").generic_string();
}

} // spt::engn
