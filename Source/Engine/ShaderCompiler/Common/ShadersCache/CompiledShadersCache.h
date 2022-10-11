#pragma once

#include "SculptorCoreTypes.h"
#include "Common/CompiledShader.h"


namespace spt::sc
{

class ShaderSourceCode;
class ShaderCompilationSettings;


class CompiledShadersCache
{
public:

	static Bool					HasCachedShader(lib::HashedString shaderRelativePath, const ShaderCompilationSettings& compilationSettings);

	static CompiledShaderFile	TryGetCachedShader(lib::HashedString shaderRelativePath, const ShaderCompilationSettings& settings);

	static CompiledShaderFile	GetCachedShaderChecked(lib::HashedString shaderRelativePath, const ShaderCompilationSettings& settings);

	static void					CacheShader(lib::HashedString shaderRelativePath, const ShaderCompilationSettings& compilationSettings, const CompiledShaderFile& shaderFile);

private:

	using HashType				= SizeType;

	static Bool					CanUseShadersCache();

	static HashType				HashShader(lib::HashedString shaderRelativePath, const ShaderCompilationSettings& compilationSettings);

	static lib::String			CreateShaderFileName(HashType hash);

	static lib::String			CreateShaderFilePath(HashType hash);
	static lib::String			CreateShaderFilePath(lib::HashedString shaderRelativePath, const ShaderCompilationSettings& compilationSettings);
};

} // spt::sc