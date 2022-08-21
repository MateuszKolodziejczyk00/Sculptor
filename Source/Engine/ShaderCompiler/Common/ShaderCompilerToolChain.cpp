#include "ShaderCompilerToolChain.h"
#include "ShaderCompilationEnvironment.h"
#include "ShadersCache/CompiledShadersCache.h"
#include "ShaderFileReader.h"
#include "Preprocessor/ShaderFilePreprocessor.h"
#include "Compiler/ShaderCompiler.h"
#include "MetaData/ShaderMetaDataBuilderTypes.h"
#include "MetaData/ShaderMetaDataBuilder.h"

namespace spt::sc
{

Bool ShaderCompilerToolChain::CompileShader(const lib::String& shaderRelativePath, const ShaderCompilationSettings& settings, CompiledShaderFile& outCompiledShaders)
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
		const lib::String shaderCode = ShaderFileReader::ReadShaderFileRelative(shaderRelativePath);

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

Bool ShaderCompilerToolChain::CompilePreprocessedShaders(const ShaderFilePreprocessingResult& preprocessingResult, const ShaderCompilationSettings& settings, const ShaderCompiler& compiler, CompiledShaderFile& outCompiledShaders)
{
	SPT_PROFILE_FUNCTION();

	Bool success = true;

	for (const PreprocessedShadersGroup& preprocessedGroup : preprocessingResult.shadersGroups)
	{
		CompiledShadersGroup& compiledShadersGroup = outCompiledShaders.groups.emplace_back(CompiledShadersGroup());

		for (const ShaderSourceCode& shaderCode : preprocessedGroup.shaders)
		{
			ShaderParametersMetaData paramsMetaData;
			CompiledShader compiledShader = compiler.CompileShader(shaderCode, settings, paramsMetaData);

			success |= compiledShader.IsValid();

			if (compiledShader.IsValid())
			{
				compiledShadersGroup.shaders.emplace_back(compiledShader);
				ShaderMetaDataBuilder::BuildShaderMetaData(compiledShader, paramsMetaData, compiledShadersGroup.metaData);
			}
		}
	}

	return success;
}

} // spt::sc
