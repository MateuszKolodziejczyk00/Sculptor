#include "ShaderCompilationEnvironment.h"

namespace spt::sc
{

namespace priv
{

static CompilationEnvironmentDef	g_environmentDef;

}

void ShaderCompilationEnvironment::Initialize(const CompilationEnvironmentDef& environmentDef)
{
	priv::g_environmentDef = environmentDef;
}

Bool ShaderCompilationEnvironment::CanCompile()
{
	return priv::g_environmentDef.targetEnvironment != ETargetEnvironment::None;
}

Bool ShaderCompilationEnvironment::ShouldGenerateDebugInfo()
{
	return priv::g_environmentDef.generateDebugInfo;
}

Bool ShaderCompilationEnvironment::ShouldUseCompiledShadersCache()
{
	return priv::g_environmentDef.useCompiledShadersCode;
}

ETargetEnvironment ShaderCompilationEnvironment::GetTargetEnvironment()
{
	return priv::g_environmentDef.targetEnvironment;
}

const lib::String& ShaderCompilationEnvironment::GetShadersPath()
{
	return priv::g_environmentDef.shadersPath;
}

const spt::lib::String& ShaderCompilationEnvironment::GetShadersCachePath()
{
	return priv::g_environmentDef.shadersCachePath;
}

const spt::lib::String& ShaderCompilationEnvironment::GetErrorLogsPath()
{
	return priv::g_environmentDef.errorLogsPath;
}

}
