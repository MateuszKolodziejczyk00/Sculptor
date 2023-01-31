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
		, compileWithDebugs(false)
		, useCompiledShadersCache(true)
		, cacheSeparateSpvFile(false)
	{ }

	ETargetEnvironment		targetEnvironment;

	Bool					generateDebugInfo;

	Bool					compileWithDebugs;

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

	static Bool						ShouldCompileWithDebugs();

	static Bool						ShouldUseCompiledShadersCache();

	static Bool						ShouldCacheSeparateSpvFile();

	static ETargetEnvironment		GetTargetEnvironment();

	static const lib::String&		GetShadersPath();

	static const lib::String&		GetShadersCachePath();

	static const lib::String&		GetErrorLogsPath();
	
};

} // spt::sc