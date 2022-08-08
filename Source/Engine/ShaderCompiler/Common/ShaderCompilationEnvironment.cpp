#include "ShaderCompilationEnvironment.h"

namespace spt::sc
{

namespace priv
{

lib::HashedString		g_shadersPath;
lib::HashedString		g_shadersCachePath;

}

void ShaderCompilationEnvironment::Initialize(const CompilationEnvironmentDef& environmentDef)
{
	priv::g_shadersPath = environmentDef.m_shadersPath;
	priv::g_shadersCachePath = environmentDef.m_shadersCachePath;
}

lib::HashedString ShaderCompilationEnvironment::GetShadersPath() const
{
	return priv::g_shadersPath;
}

}
