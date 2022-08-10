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
	return priv::g_environmentDef.m_targetEnvironment != ETargetEnvironment::None;
}

Bool ShaderCompilationEnvironment::ShouldGenerateDebugInfo()
{
	return priv::g_environmentDef.m_generateDebugInfo;
}

Bool ShaderCompilationEnvironment::ShouldUseCompiledShadersCache()
{
	return priv::g_environmentDef.m_useCompiledShadersCode;
}

ETargetEnvironment ShaderCompilationEnvironment::GetTargetEnvironment()
{
	return priv::g_environmentDef.m_targetEnvironment;
}

const lib::String& ShaderCompilationEnvironment::GetShadersPath()
{
	return priv::g_environmentDef.m_shadersPath;
}

const spt::lib::String& ShaderCompilationEnvironment::GetShadersCachePath()
{
	return priv::g_environmentDef.m_shadersCachePath;
}

const spt::lib::String& ShaderCompilationEnvironment::GetErrorLogsPath()
{
	return priv::g_environmentDef.m_errorLogsPath;
}

}
