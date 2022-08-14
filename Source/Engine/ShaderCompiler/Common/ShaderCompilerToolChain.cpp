#include "ShaderCompilerToolChain.h"
#include "ShaderCompilationEnvironment.h"
#include "CompiledShadersCache.h"
#include "ShaderFileReader.h"
#include "Preprocessor/ShaderFilePreprocessor.h"
#include "Compiler/ShaderCompiler.h"

namespace spt::sc
{

Bool ShaderCompilerToolChain::CompileShader(lib::HashedString shaderRelativePath, const ShaderCompilationSettings& settings, CompiledShaderFile& outCompiledShaders)
{
	SPT_PROFILE_FUNCTION();

	Bool compilationResult = false;

	const Bool shouldUseShadersCache = ShaderCompilationEnvironment::ShouldUseCompiledShadersCache();

	if (shouldUseShadersCache)
	{
		outCompiledShaders = CompiledShadersCache::TryGetCachedShader(shaderRelativePath, settings);
	}

	if(!outCompiledShaders.IsValid())
	{
		const lib::String shaderCode = ShaderFileReader::ReadShaderFile(shaderRelativePath);

		const ShaderFilePreprocessingResult preprocessingResult = ShaderFilePreprocessor::PreprocessShaderFileSourceCode(shaderCode);

		if (preprocessingResult.IsValid())
		{
			ShaderCompiler compiler;

			compilationResult = CompilePreprocessedShaders(preprocessingResult, settings, compiler, outCompiledShaders);

			if (compilationResult && shouldUseShadersCache)
			{
				CompiledShadersCache::CacheShader(shaderRelativePath, settings, outCompiledShaders);
			}
		}
	}
	else
	{
		compilationResult = true;
	}

	return compilationResult;
}

Bool ShaderCompilerToolChain::CompilePreprocessedShaders(const ShaderFilePreprocessingResult& preprocessingResult, const ShaderCompilationSettings& settings, ShaderCompiler& compiler, CompiledShaderFile& outCompiledShaders)
{
	SPT_PROFILE_FUNCTION();

	Bool success = true;

	for (const PreprocessedShadersGroup& preprocessedGroup : preprocessingResult.m_shadersGroups)
	{
		CompiledShadersGroup& compiledShadersGroup = outCompiledShaders.m_groups.emplace_back(CompiledShadersGroup());

		for (const ShaderSourceCode& shaderCode : preprocessedGroup.m_shaders)
		{
			CompiledShader compiledShader = compiler.CompileShader(shaderCode, settings);

			success |= compiledShader.IsValid();

			if (compiledShader.IsValid())
			{
				compiledShadersGroup.m_shaders.emplace_back(compiledShader);
			}
		}
	}

	return success;
}

} // spt::sc
