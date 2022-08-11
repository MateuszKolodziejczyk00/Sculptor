#pragma once

#include "SculptorCoreTypes.h"
#include "CompiledShader.h"


namespace spt::sc
{

class ShaderSourceCode;
class ShaderCompilationSettings;


class CompiledShadersCache
{
public:

	static Bool				HasCachedShader(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings);

	static CompiledShader	TryGetCachedShader(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings);

	static CompiledShader	GetCachedShaderChecked(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings);

	static void				CacheShader(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings, const CompiledShader& shader);

private:

	using HashType			= SizeType;

	static Bool				CanUseShadersCache();

	static HashType			HashShader(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings);

	static lib::String		CreateShaderFileName(HashType hash);

	static lib::String		CreateShaderFilePath(HashType hash);
	static lib::String		CreateShaderFilePath(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings);
};

}