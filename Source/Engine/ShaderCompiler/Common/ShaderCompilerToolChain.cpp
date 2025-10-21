#include "ShaderCompilerToolChain.h"
#include "ShaderCompilationEnvironment.h"
#include "ShadersCache/CompiledShadersCache.h"
#include "ShaderFileReader.h"
#include "Compiler/ShaderCompiler.h"
#include "MetaData/ShaderMetaDataBuilderTypes.h"
#include "MetaData/ShaderMetaDataBuilder.h"

namespace spt::sc
{

CompiledShader ShaderCompilerToolChain::CompileShader(const lib::String& shaderRelativePath, const ShaderStageCompilationDef& shaderStageDef, const ShaderCompilationSettings& compilationSettings, EShaderCompilationFlags compilationFlags)
{
	SPT_PROFILER_FUNCTION();

	const Bool shouldUseShadersCache = ShaderCompilationEnvironment::ShouldUseCompiledShadersCache();

	const Bool updateOnly = lib::HasAnyFlag(compilationFlags, EShaderCompilationFlags::UpdateOnly);

	CompiledShader outCompiledShader;

	if (shouldUseShadersCache && !updateOnly)
	{
		outCompiledShader = CompiledShadersCache::TryGetCachedShader(shaderRelativePath, shaderStageDef, compilationSettings);
	}

	// Early out. We don't want to compile shader if UpdateOnly is set to true and shader in cache is up to date
	if (updateOnly && CompiledShadersCache::IsCachedShaderUpToDate(shaderRelativePath, shaderStageDef, compilationSettings))
	{
		return outCompiledShader;
	}

	if(!outCompiledShader.IsValid())
	{
		const lib::String shaderCode = ShaderFileReader::ReadShaderFileRelative(shaderRelativePath);
			
		ShaderCompiler compiler;

		outCompiledShader = CompilePreprocessedShaders(shaderRelativePath, shaderCode, shaderStageDef, compilationSettings, compiler);

		if (outCompiledShader.IsValid() && shouldUseShadersCache)
		{
			CompiledShadersCache::CacheShader(shaderRelativePath, shaderStageDef, compilationSettings, outCompiledShader);
		}
	}

	return outCompiledShader;
}

CompiledShader ShaderCompilerToolChain::CompilePreprocessedShaders(const lib::String& shaderRelativePath, const lib::String& shaderCode, const ShaderStageCompilationDef& shaderStageDef, const ShaderCompilationSettings& compilationSettings, const ShaderCompiler& compiler)
{
	SPT_PROFILER_FUNCTION();

	ShaderCompilationMetaData compilationMetaData;
	CompiledShader compiledShader = compiler.CompileShader(shaderRelativePath, shaderCode, shaderStageDef, compilationSettings, compilationMetaData);

	if (compiledShader.IsValid())
	{
		ShaderMetaDataBuilder::BuildShaderMetaData(compiledShader, compilationMetaData, compiledShader.metaData);

#if SPT_SHADERS_DEBUG_FEATURES
		compiledShader.debugMetaData = compilationMetaData.GetDebugMetaData();
#endif // SPT_SHADERS_DEBUG_FEATURES

#if WITH_SHADERS_HOT_RELOAD
		compiledShader.fileDependencies = compilationMetaData.GetFileDependencies();
#endif // WITH_SHADERS_HOT_RELOAD
	}

	return compiledShader;
}

} // spt::sc
