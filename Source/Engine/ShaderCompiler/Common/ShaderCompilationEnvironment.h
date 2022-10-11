#pragma once

#include "ShaderCompilerMacros.h"
#include "SculptorCoreTypes.h"
#include "ShaderCompilerTypes.h"


namespace spt::sc
{


struct CompilationEnvironmentDef
{
	CompilationEnvironmentDef()
		: targetEnvironment(ETargetEnvironment::None)
		, generateDebugInfo(false)
		, useCompiledShadersCache(true)
		, cacheSeparateSpvFile(false)
	{ }

	ETargetEnvironment		targetEnvironment;

	Bool					generateDebugInfo;

	Bool					useCompiledShadersCache;

	/** if true, additional, separate .spv file will be generated when shader will be cached */
	Bool					cacheSeparateSpvFile;

	lib::String				shadersPath;

	lib::String				shadersCachePath;

	lib::String				errorLogsPath;
};


class SHADER_COMPILER_API ShaderCompilationEnvironment
{
public:

	static void						Initialize(const CompilationEnvironmentDef& environmentDef);

	static Bool						CanCompile();

	static Bool						ShouldGenerateDebugInfo();

	static Bool						ShouldUseCompiledShadersCache();

	static Bool						ShouldCacheSeparateSpvFile();

	static ETargetEnvironment		GetTargetEnvironment();

	static const lib::String&		GetShadersPath();

	static const lib::String&		GetShadersCachePath();

	static const lib::String&		GetErrorLogsPath();
	
};

} // spt::sc