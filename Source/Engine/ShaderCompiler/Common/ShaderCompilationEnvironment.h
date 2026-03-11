#pragma once

#include "ShaderCompilerMacros.h"
#include "SculptorCoreTypes.h"
#include "ShaderCompilerTypes.h"
#include "Serialization.h"


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

	lib::Path				shadersPath;

	lib::Path				shadersCachePath;

	lib::Path				errorLogsPath;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("TargetEnvironment", targetEnvironment);
		serializer.Serialize("GenerateDebugInfo", generateDebugInfo);
		serializer.Serialize("CompileWithDebugs", compileWithDebugs);
		serializer.Serialize("UseCompiledShadersCache", useCompiledShadersCache);
		serializer.Serialize("CacheSeparateSpvFile", cacheSeparateSpvFile);
		serializer.Serialize("ShadersPath", shadersPath);
		serializer.Serialize("ShadersCachePath", shadersCachePath);
		serializer.Serialize("ErrorLogsPath", errorLogsPath);
	}
};


class SHADER_COMPILER_API ShaderCompilationEnvironment
{
public:

	static void							Initialize(const CompilationEnvironmentDef& environmentDef);
	static CompilationEnvironmentDef*	GetCompilationEnvironmentDef();
	static void							InitializeModule(CompilationEnvironmentDef* mainEnvironmentDef);

	static Bool						CanCompile();

	static Bool						ShouldGenerateDebugInfo();

	static Bool						ShouldCompileWithDebugs();

	static Bool						ShouldUseCompiledShadersCache();

	static Bool						ShouldCacheSeparateSpvFile();

	static ETargetEnvironment		GetTargetEnvironment();

	static const lib::Path&			GetShadersPath();

	static const lib::Path&			GetShadersCachePath();

	static const lib::Path&			GetErrorLogsPath();
	
};

} // spt::sc
