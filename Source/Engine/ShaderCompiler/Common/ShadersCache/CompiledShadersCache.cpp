#include "CompiledShadersCache.h"
#include "Common/ShaderCompilationEnvironment.h"
#include "Common/ShaderCompilationInput.h"
#include "FileSystem/File.h"
#include "Utility/String/StringUtils.h"
#include "Paths.h"
#include "SerializationHelper.h"


namespace spt::sc
{

Bool CompiledShadersCache::HasCachedShader(lib::HashedString shaderRelativePath, const ShaderStageCompilationDef& shaderStageDef, const ShaderCompilationSettings& compilationSettings)
{
	SPT_PROFILER_FUNCTION();

	if (!CanUseShadersCache())
	{
		return false;
	}

	const lib::String filePath = CreateShaderFilePath(shaderRelativePath, shaderStageDef, compilationSettings);
	return lib::File::Exists(filePath);
}

CompiledShader CompiledShadersCache::TryGetCachedShader(lib::HashedString shaderRelativePath, const ShaderStageCompilationDef& shaderStageDef, const ShaderCompilationSettings& compilationSettings)
{
	SPT_PROFILER_FUNCTION();

	CompiledShader compiledShader;

	if (CanUseShadersCache())
	{
		const lib::String cachedShaderPath = CreateShaderFilePath(shaderRelativePath, shaderStageDef, compilationSettings);
		const lib::String shaderSourcePath = CreateShaderSourceCodeFilePath(shaderRelativePath);

		const Bool isCachedShaderUpToDate = IsCachedShaderUpToDateImpl(cachedShaderPath, shaderSourcePath);

		if (isCachedShaderUpToDate)
		{
			srl::SerializationHelper::LoadTextStructFromFile(compiledShader, cachedShaderPath);
		}

		if (compiledShader.IsValid() && !IsDeserializedShaderUpToDateImpl(cachedShaderPath, compiledShader))
		{
			compiledShader = CompiledShader{};
		}
	}

	return compiledShader;
}

CompiledShader CompiledShadersCache::GetCachedShaderChecked(lib::HashedString shaderRelativePath, const ShaderStageCompilationDef& shaderStageDef, const ShaderCompilationSettings& compilationSettings)
{
	CompiledShader compiledShader = TryGetCachedShader(shaderRelativePath, shaderStageDef, compilationSettings);
	SPT_CHECK(compiledShader.IsValid());
	return compiledShader;
}

void CompiledShadersCache::CacheShader(lib::HashedString shaderRelativePath, const ShaderStageCompilationDef& shaderStageDef, const ShaderCompilationSettings& compilationSettings, const CompiledShader& shader)
{
	SPT_PROFILER_FUNCTION();
	
	SPT_CHECK(CanUseShadersCache());

	const lib::String filePath = CreateShaderFilePath(shaderRelativePath, shaderStageDef, compilationSettings);
	srl::SerializationHelper::SaveTextStructToFile(shader, filePath);

	if (ShaderCompilationEnvironment::ShouldCacheSeparateSpvFile())
	{
		const lib::String binaryPath = filePath + '_' + std::to_string(static_cast<Uint32>(shader.stage)) + ".spv";
		const CompiledShader::Binary& bin = shader.binary;
		srl::SerializationHelper::SaveBinaryToFile(reinterpret_cast<const Byte*>(bin.data()), bin.size() * 4, binaryPath);
	}
}

Bool CompiledShadersCache::IsCachedShaderUpToDate(lib::HashedString shaderRelativePath, const ShaderStageCompilationDef& shaderStageDef, const ShaderCompilationSettings& compilationSettings)
{
	if (!CanUseShadersCache())
	{
		return false;
	}

	const lib::String cachedShaderPath = CreateShaderFilePath(shaderRelativePath, shaderStageDef, compilationSettings);
	const lib::String shaderSourcePath = CreateShaderSourceCodeFilePath(shaderRelativePath);

	if (!IsCachedShaderUpToDateImpl(cachedShaderPath, shaderSourcePath))
	{
		return false;
	}

	CompiledShader compiledShader;
	srl::SerializationHelper::LoadTextStructFromFile(compiledShader, cachedShaderPath);

	return compiledShader.IsValid() && IsDeserializedShaderUpToDateImpl(cachedShaderPath, compiledShader);
}

Bool CompiledShadersCache::CanUseShadersCache()
{
	return ShaderCompilationEnvironment::ShouldUseCompiledShadersCache();
}

CompiledShadersCache::HashType CompiledShadersCache::HashShader(lib::HashedString shaderRelativePath, const ShaderStageCompilationDef& shaderStageDef, const ShaderCompilationSettings& compilationSettings)
{
	SPT_PROFILER_FUNCTION();

	return lib::HashCombine(shaderRelativePath.GetKey(),
						   shaderStageDef.Hash(),
						   compilationSettings.Hash());
}

lib::String CompiledShadersCache::CreateShaderFileName(HashType hash)
{
	return lib::StringUtils::ToHexString(reinterpret_cast<const Byte*>(&hash), sizeof(HashType));
}

lib::String CompiledShadersCache::CreateShaderFilePath(HashType hash)
{
	return engn::Paths::Combine(ShaderCompilationEnvironment::GetShadersCachePath(), CreateShaderFileName(hash));
}

lib::String CompiledShadersCache::CreateShaderSourceCodeFilePath(lib::HashedString shaderRelativePath)
{
	return engn::Paths::Combine(ShaderCompilationEnvironment::GetShadersPath(), shaderRelativePath.GetView());;
}

lib::String CompiledShadersCache::CreateShaderFilePath(lib::HashedString shaderRelativePath, const ShaderStageCompilationDef& shaderStageDef, const ShaderCompilationSettings& compilationSettings)
{
	SPT_PROFILER_FUNCTION();
	
	const HashType hash = HashShader(shaderRelativePath, shaderStageDef, compilationSettings);
	return CreateShaderFilePath(hash);
}

Bool CompiledShadersCache::IsCachedShaderUpToDateImpl(const lib::String& cachedShaderPath, const lib::String& shaderSourceCodePath)
{
	Bool isCachedShaderUpToDate = false;

	if (lib::File::Exists(cachedShaderPath))
	{
		const auto cachedShaderWriteTime = std::filesystem::last_write_time(cachedShaderPath);
		const auto shaderSourceWriteTime = std::filesystem::last_write_time(shaderSourceCodePath);

		isCachedShaderUpToDate = cachedShaderWriteTime > shaderSourceWriteTime;
	}

	return isCachedShaderUpToDate;
}

Bool CompiledShadersCache::IsDeserializedShaderUpToDateImpl(const lib::String& cachedShaderPath, const CompiledShader& shader)
{
#if WITH_SHADERS_HOT_RELOAD
	const auto cachedShaderWriteTime = std::filesystem::last_write_time(cachedShaderPath);

	for (const lib::String& dependencyPath : shader.fileDependencies)
	{
		if (!lib::File::Exists(dependencyPath))
		{
			return false;
		}

		const auto dependencyWriteTime = std::filesystem::last_write_time(dependencyPath);

		if (dependencyWriteTime > cachedShaderWriteTime)
		{
			return false;
		}
	}
#endif // WITH_SHADERS_HOT_RELOAD

	return true;
}

} // spt::sc
