#include "ShaderCompilerToolChain.h"
#include "ShaderCompilationEnvironment.h"
#include "ShadersCache/CompiledShadersCache.h"
#include "ShaderFileReader.h"
#include "Preprocessor/ShaderFilePreprocessor.h"
#include "Compiler/ShaderCompiler.h"
#include "SPIRVOptimizer.h"
#include "MetaData/ShaderMetaDataBuilderTypes.h"
#include "MetaData/ShaderMetaDataBuilder.h"

namespace spt::sc
{

Bool ShaderCompilerToolChain::CompileShader(const lib::String& shaderRelativePath, const ShaderCompilationSettings& settings, EShaderCompilationFlags compilationFlags, CompiledShaderFile& outCompiledShaders)
{
	SPT_PROFILER_FUNCTION();

	Bool compilationResult = false;

	const Bool shouldUseShadersCache = ShaderCompilationEnvironment::ShouldUseCompiledShadersCache();

	const Bool updateOnly = lib::HasAnyFlag(compilationFlags, EShaderCompilationFlags::UpdateOnly);

	if (shouldUseShadersCache && !updateOnly)
	{
		outCompiledShaders = CompiledShadersCache::TryGetCachedShader(shaderRelativePath, settings);
		compilationResult = outCompiledShaders.IsValid();
	}

	// Early out. We don't want to compile shader if UpdateOnly is set to true and shader in cache is up to date
	if (updateOnly && CompiledShadersCache::IsCachedShaderUpToDate(shaderRelativePath, settings))
	{
		return compilationResult;
	}

	if(!compilationResult)
	{
		const lib::String shaderCode = ShaderFileReader::ReadShaderFileRelative(shaderRelativePath);

		const ShaderFilePreprocessingResult preprocessingResult = ShaderFilePreprocessor::PreprocessShaderFileSourceCode(shaderCode);

		if (preprocessingResult.IsValid())
		{
			ShaderCompiler compiler;

			compilationResult = CompilePreprocessedShaders(shaderRelativePath, preprocessingResult, settings, compiler, outCompiledShaders);

			if (compilationResult && shouldUseShadersCache)
			{
				CompiledShadersCache::CacheShader(shaderRelativePath, settings, outCompiledShaders);
			}
		}
	}

	return compilationResult;
}

Bool ShaderCompilerToolChain::CompilePreprocessedShaders(const lib::String& shaderRelativePath, const ShaderFilePreprocessingResult& preprocessingResult, const ShaderCompilationSettings& settings, const ShaderCompiler& compiler, CompiledShaderFile& outCompiledShaders)
{
	SPT_PROFILER_FUNCTION();

	Bool success = true;

	for (const ShaderSourceCode& shaderCode : preprocessingResult.shaders)
	{
		ShaderParametersMetaData paramsMetaData;
		CompiledShader compiledShader = compiler.CompileShader(shaderRelativePath, shaderCode, settings, paramsMetaData);

		success &= compiledShader.IsValid();

		if (success)
		{
			CompiledShader::Binary optimizedBinary = SPIRVOptimizer::OptimizeBinary(compiledShader.GetBinary());
			compiledShader.SetBinary(std::move(optimizedBinary));
		}

		if (compiledShader.IsValid())
		{
			outCompiledShaders.shaders.emplace_back(compiledShader);
			ShaderMetaDataBuilder::BuildShaderMetaData(compiledShader, paramsMetaData, outCompiledShaders.metaData);
		}
	}

	return success;
}

} // spt::sc
