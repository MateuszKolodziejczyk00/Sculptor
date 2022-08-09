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

ETargetEnvironment ShaderCompilationEnvironment::GetTargetEnvironment()
{
	return priv::g_environmentDef.m_targetEnvironment;
}

lib::HashedString ShaderCompilationEnvironment::GetShadersPath()
{
	return priv::g_environmentDef.m_shadersPath;
}

}
