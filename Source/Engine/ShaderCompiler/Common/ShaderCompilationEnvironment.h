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

	lib::String				m_shadersPath;

	lib::String				m_shadersCachePath;

	lib::String				m_errorLogsPath;
};


class SHADER_COMPILER_API ShaderCompilationEnvironment
{
public:

	static void						Initialize(const CompilationEnvironmentDef& environmentDef);

	static Bool						CanCompile();

	static Bool						ShouldGenerateDebugInfo();

	static ETargetEnvironment		GetTargetEnvironment();

	static const lib::String&		GetShadersPath();

	static const lib::String&		GetShadersCachePath();

	static const lib::String&		GetErrorLogsPath();
	
};

}