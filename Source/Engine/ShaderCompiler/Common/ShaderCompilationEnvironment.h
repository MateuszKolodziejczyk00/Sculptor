#pragma once

#include "ShaderCompilerMacros.h"
#include "SculptorCoreTypes.h"
#include "ShaderCompilerTypes.h"


namespace spt::sc
{


struct CompilationEnvironmentDef
{
	ETargetEnvironment		m_targetEnvironment;

	lib::HashedString		m_shadersPath;

	lib::HashedString		m_shadersCachePath;
};


class SHADER_COMPILER_API ShaderCompilationEnvironment
{
public:

	void					Initialize(const CompilationEnvironmentDef& environmentDef);

	lib::HashedString		GetShadersPath() const;
	
};

}