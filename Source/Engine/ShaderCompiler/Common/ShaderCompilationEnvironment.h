#pragma once

#include "ShaderCompilerMacros.h"
#include "SculptorCoreTypes.h"
#include "ShaderCompilerTypes.h"


namespace spt::sc
{


struct CompilationEnvironmentDef
{
	ETargetEnvironment		m_targetEnvironment;

	Bool					m_generateDebugInfo;

	lib::HashedString		m_shadersPath;

	lib::HashedString		m_shadersCachePath;
};


class SHADER_COMPILER_API ShaderCompilationEnvironment
{
public:

	static void						Initialize(const CompilationEnvironmentDef& environmentDef);

	static Bool						CanCompile();

	static Bool						ShouldGenerateDebugInfo();

	static ETargetEnvironment		GetTargetEnvironment();

	static lib::HashedString		GetShadersPath();
	
};

}