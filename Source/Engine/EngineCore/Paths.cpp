#include "Paths.h"
#include "Utils/CommandLineArguments.h"


namespace spt::engn
{

namespace params
{

static const lib::HashedString enginePath = "EngineRelativePath";

} // params

namespace priv
{

static lib::String	g_enginePath;
static lib::String	g_configsPath;
static lib::String	g_tracesPath;

} // priv

void Paths::Initialize(const CommandLineArguments& cmdLineArgs)
{
	priv::g_enginePath = cmdLineArgs.GetValue(params::enginePath).ToString();

	priv::g_configsPath = Combine(priv::g_enginePath, "Config");
	priv::g_tracesPath = Combine(priv::g_enginePath, "Saved/Traces");
}

const lib::String& Paths::GetEnginePath()
{
	return priv::g_enginePath;
}

const lib::String& Paths::GetConfigsPath()
{
	return priv::g_configsPath;
}

const lib::String& Paths::GetTracesPath()
{
	return priv::g_tracesPath;
}

void Paths::AppendPath(lib::String& path, lib::StringView pathToAppend)
{
	SPT_PROFILE_FUNCTION();

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
	SPT_PROFILE_FUNCTION();

	lib::String path(pathBegin);
	AppendPath(path, pathEnd);
	return path;
}

} // spt::engn