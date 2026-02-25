#include "Paths.h"
#include "FileSystem/File.h"
#include "Utils/CommandLineArguments.h"


namespace spt::engn
{

namespace params
{

static const char* enginePath = "-EngineRelativePath";

} // params

namespace priv
{

static lib::String	g_enginePath;
static lib::String	g_configsPath;
static lib::String	g_savedPath;
static lib::String	g_tracesPath;
static lib::String	g_gpuCrashDumpsPath;
static lib::String	g_contentPath;
static lib::String	g_imGuiConfigPath;
static lib::String	g_executablePath;
static lib::String	g_executableDirectory;

} // priv

void Paths::Initialize(const CommandLineArguments& cmdLineArgs)
{
	priv::g_enginePath = cmdLineArgs.GetValue(params::enginePath).ToString();

	priv::g_configsPath         = Combine(priv::g_enginePath, "Config");
	priv::g_tracesPath          = Combine(priv::g_enginePath, "Saved/Traces");
	priv::g_savedPath           = Combine(priv::g_enginePath, "Saved");
	priv::g_gpuCrashDumpsPath   = Combine(priv::g_enginePath, "Saved/GPUCrashDumps");
	priv::g_contentPath         = Combine(priv::g_enginePath, "Content");
	priv::g_imGuiConfigPath     = Combine(priv::g_configsPath, "ImGuiConfig.ini");
	priv::g_executablePath      = platf::Platform::GetExecutablePath();
	priv::g_executableDirectory = lib::Path(priv::g_executablePath).parent_path().string();
}

const lib::String& Paths::GetEnginePath()
{
	return priv::g_enginePath;
}

const lib::String& Paths::GetConfigsPath()
{
	return priv::g_configsPath;
}

const lib::String& Paths::GetSavedPath()
{
	return priv::g_savedPath;
}

const lib::String& Paths::GetTracesPath()
{
	return priv::g_tracesPath;
}

const lib::String& Paths::GetGPUCrashDumpsPath()
{
	return priv::g_gpuCrashDumpsPath;
}

const lib::String& Paths::GetContentPath()
{
	return priv::g_contentPath;
}

const lib::String& Paths::GetImGuiConfigPath()
{
	return priv::g_imGuiConfigPath;
}

const lib::StringView Paths::GetExtension(lib::StringView path)
{
	const SizeType dotPosition = path.rfind('.');
	return dotPosition != lib::StringView::npos ? lib::StringView(std::begin(path) + dotPosition + 1, std::end(path)) : lib::StringView();
}

const lib::String& Paths::GetExecutablePath()
{
	return priv::g_executablePath;
}

const lib::String& Paths::GetExecutableDirectory()
{
	return priv::g_executableDirectory;
}

void Paths::AppendPath(lib::String& path, lib::StringView pathToAppend)
{
	SPT_PROFILER_FUNCTION();

	const auto isSlash = [](char c) { return c == '/' || c == '\\'; };

	const Bool shouldAppendSlash = !path.empty() && !pathToAppend.empty() && !isSlash(path.back()) && !isSlash(pathToAppend.front());

	if (shouldAppendSlash)
	{
		path += '/';
	}

	path += pathToAppend;
}

lib::String Paths::Combine(lib::StringView pathBegin, lib::StringView pathEnd)
{
	SPT_PROFILER_FUNCTION();

	lib::String path(pathBegin);
	AppendPath(path, pathEnd);
	return path;
}

} // spt::engn
