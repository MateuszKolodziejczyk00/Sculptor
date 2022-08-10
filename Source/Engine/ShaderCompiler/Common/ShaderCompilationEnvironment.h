#pragma once

#include "ShaderCompilerMacros.h"
#include "SculptorCoreTypes.h"
#include "ShaderCompilerTypes.h"


namespace spt::sc
{


struct CompilationEnvironmentDef
{
	CompilationEnvironmentDef()
		: m_targetEnvironment(ETargetEnvironment::None)
		, m_generateDebugInfo(false)
		, m_useCompiledShadersCode(true)
	{ }

	ETargetEnvironment		m_targetEnvironment;

	Bool					m_generateDebugInfo;

	Bool					m_useCompiledShadersCode;

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

	static Bool						ShouldUseCompiledShadersCache();

	static ETargetEnvironment		GetTargetEnvironment();

	static const lib::String&		GetShadersPath();

	static const lib::String&		GetShadersCachePath();

	static const lib::String&		GetErrorLogsPath();
	
};

}