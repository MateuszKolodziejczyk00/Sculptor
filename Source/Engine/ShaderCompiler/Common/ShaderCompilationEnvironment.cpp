#include "ShaderCompilationEnvironment.h"
#include "EngineCore/Engine.h"
#include "Paths.h"

namespace spt::sc
{

namespace priv
{

static CompilationEnvironmentDef	g_environmentDef;

} // priv

void ShaderCompilationEnvironment::Initialize(const CompilationEnvironmentDef& environmentDef)
{
	priv::g_environmentDef = environmentDef;

	// add relative path to engine as prefix (path in definition is relative to the engine directory)
	const lib::String& relativePathToEngine = engn::Paths::GetEnginePath();
	priv::g_environmentDef.shadersPath		= engn::Paths::Combine(relativePathToEngine, priv::g_environmentDef.shadersPath);
	priv::g_environmentDef.shadersCachePath	= engn::Paths::Combine(relativePathToEngine, priv::g_environmentDef.shadersCachePath);
	priv::g_environmentDef.errorLogsPath	= engn::Paths::Combine(relativePathToEngine, priv::g_environmentDef.errorLogsPath);
}

Bool ShaderCompilationEnvironment::CanCompile()
{
	return priv::g_environmentDef.targetEnvironment != ETargetEnvironment::None;
}

Bool ShaderCompilationEnvironment::ShouldGenerateDebugInfo()
{
	return priv::g_environmentDef.generateDebugInfo;
}

Bool ShaderCompilationEnvironment::ShouldCompileWithDebugs()
{
	return priv::g_environmentDef.compileWithDebugs;
}

Bool ShaderCompilationEnvironment::ShouldUseCompiledShadersCache()
{
	return priv::g_environmentDef.useCompiledShadersCache;
}

Bool ShaderCompilationEnvironment::ShouldCacheSeparateSpvFile()
{
	return priv::g_environmentDef.cacheSeparateSpvFile;
}

ETargetEnvironment ShaderCompilationEnvironment::GetTargetEnvironment()
{
	return priv::g_environmentDef.targetEnvironment;
}

const lib::String& ShaderCompilationEnvironment::GetShadersPath()
{
	return priv::g_environmentDef.shadersPath;
}

const lib::String& ShaderCompilationEnvironment::GetShadersCachePath()
{
	return priv::g_environmentDef.shadersCachePath;
}

const lib::String& ShaderCompilationEnvironment::GetErrorLogsPath()
{
	return priv::g_environmentDef.errorLogsPath;
}

} // spt::sc
