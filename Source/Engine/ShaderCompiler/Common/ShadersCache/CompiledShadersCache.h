#pragma once

#include "SculptorCoreTypes.h"
#include "Common/CompiledShader.h"


namespace spt::sc
{

struct ShaderStageCompilationDef;
class ShaderCompilationSettings;


class CompiledShadersCache
{
public:

	static Bool					HasCachedShader(lib::HashedString shaderRelativePath, const ShaderStageCompilationDef& shaderStageDef, const ShaderCompilationSettings& compilationSettings);

	static CompiledShader		TryGetCachedShader(lib::HashedString shaderRelativePath, const ShaderStageCompilationDef& shaderStageDef, const ShaderCompilationSettings& compilationSettings);

	static CompiledShader		GetCachedShaderChecked(lib::HashedString shaderRelativePath, const ShaderStageCompilationDef& shaderStageDef, const ShaderCompilationSettings& compilationSettings);

	static void					CacheShader(lib::HashedString shaderRelativePath, const ShaderStageCompilationDef& shaderStageDef, const ShaderCompilationSettings& compilationSettings, const CompiledShader& shaderFile);

	static Bool					IsCachedShaderUpToDate(lib::HashedString shaderRelativePath, const ShaderStageCompilationDef& shaderStageDef, const ShaderCompilationSettings& compilationSettings);

private:

	using HashType				= SizeType;

	static Bool					CanUseShadersCache();

	static HashType				HashShader(lib::HashedString shaderRelativePath, const ShaderStageCompilationDef& shaderStageDef, const ShaderCompilationSettings& compilationSettings);

	static lib::String			CreateShaderFileName(HashType hash);

	static lib::String			CreateShaderFilePath(HashType hash);
	static lib::String			CreateShaderFilePath(lib::HashedString shaderRelativePath, const ShaderStageCompilationDef& shaderStageDef, const ShaderCompilationSettings& compilationSettings);
	static lib::String			CreateShaderSourceCodeFilePath(lib::HashedString shaderRelativePath);

	static Bool					IsCachedShaderUpToDateImpl(const lib::String& cachedShaderPath, const lib::String& shaderSourceCodePath);
	static Bool					IsDeserializedShaderUpToDateImpl(const lib::String& cachedShaderPath, const CompiledShader& shader);
};

} // spt::sc