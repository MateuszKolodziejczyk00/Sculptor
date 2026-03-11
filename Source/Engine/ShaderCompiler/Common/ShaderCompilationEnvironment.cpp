#include "ShaderCompilationEnvironment.h"
#include "Engine.h"

namespace spt::sc
{

namespace priv
{

static CompilationEnvironmentDef* g_environmentDef = nullptr;

} // priv

void ShaderCompilationEnvironment::Initialize(const CompilationEnvironmentDef& environmentDef)
{
	priv::g_environmentDef = new CompilationEnvironmentDef(environmentDef);

	const engn::Paths& paths = engn::Engine::Get().GetPaths();

	// add relative path to engine as prefix (path in definition is relative to the engine directory)
	const lib::Path& relativePathToEngine = paths.enginePath;
	priv::g_environmentDef->shadersPath      = relativePathToEngine / priv::g_environmentDef->shadersPath;
	priv::g_environmentDef->shadersCachePath = relativePathToEngine / priv::g_environmentDef->shadersCachePath;
	priv::g_environmentDef->errorLogsPath    = relativePathToEngine / priv::g_environmentDef->errorLogsPath;
}

CompilationEnvironmentDef* ShaderCompilationEnvironment::GetCompilationEnvironmentDef()
{
	return priv::g_environmentDef;
}

void ShaderCompilationEnvironment::InitializeModule(CompilationEnvironmentDef* mainEnvironmentDef)
{
	SPT_CHECK(!priv::g_environmentDef);
	priv::g_environmentDef = mainEnvironmentDef;
}

Bool ShaderCompilationEnvironment::CanCompile()
{
	return priv::g_environmentDef->targetEnvironment != ETargetEnvironment::None;
}

Bool ShaderCompilationEnvironment::ShouldGenerateDebugInfo()
{
	return priv::g_environmentDef->generateDebugInfo;
}

Bool ShaderCompilationEnvironment::ShouldCompileWithDebugs()
{
	return priv::g_environmentDef->compileWithDebugs;
}

Bool ShaderCompilationEnvironment::ShouldUseCompiledShadersCache()
{
	return priv::g_environmentDef->useCompiledShadersCache;
}

Bool ShaderCompilationEnvironment::ShouldCacheSeparateSpvFile()
{
	return priv::g_environmentDef->cacheSeparateSpvFile;
}

ETargetEnvironment ShaderCompilationEnvironment::GetTargetEnvironment()
{
	return priv::g_environmentDef->targetEnvironment;
}

const lib::Path& ShaderCompilationEnvironment::GetShadersPath()
{
	return priv::g_environmentDef->shadersPath;
}

const lib::Path& ShaderCompilationEnvironment::GetShadersCachePath()
{
	return priv::g_environmentDef->shadersCachePath;
}

const lib::Path& ShaderCompilationEnvironment::GetErrorLogsPath()
{
	return priv::g_environmentDef->errorLogsPath;
}

} // spt::sc
