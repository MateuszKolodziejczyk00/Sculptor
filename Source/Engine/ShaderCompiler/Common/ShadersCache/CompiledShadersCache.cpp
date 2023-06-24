#include "CompiledShadersCache.h"
#include "Common/ShaderCompilationEnvironment.h"
#include "Common/ShaderCompilationInput.h"
#include "FileSystem/File.h"
#include "Utility/String/StringUtils.h"
#include "Paths.h"

#include "SculptorYAML.h"
#include "SerializationHelper.h"
#include "ShaderMetaDataTypesSerialization.h"

#include <filesystem>

namespace spt::srl
{

// spt::sc::CompiledShader ==============================================================

template<>
struct TypeSerializer<sc::CompiledShader>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& shader)
	{
		using ShaderBinary			= typename Param::Binary;
		using ShaderBinaryElement	= typename ShaderBinary::value_type;

		if constexpr (Serializer::IsSaving())
		{
			const srl::Binary binary(reinterpret_cast<const unsigned char*>(shader.binary.data()), shader.binary.size() * sizeof(ShaderBinaryElement));
			serializer.Serialize("Binary", binary);
		}
		else
		{
			srl::Binary binary;
			serializer.Serialize("Binary", binary);

			SPT_CHECK(binary.size() % sizeof(ShaderBinaryElement) == 0);

			ShaderBinary shaderBinary(binary.size() / sizeof(ShaderBinaryElement));
			memcpy_s(shaderBinary.data(), binary.size(), binary.data(), binary.size());

			shader.binary = std::move(shaderBinary);
		}
		
		serializer.SerializeEnum("Stage", shader.stage);
		serializer.Serialize("EntryPoint", shader.entryPoint);
			
		serializer.Serialize("MetaData", shader.metaData);
	}
};

} // spt::srl

SPT_YAML_SERIALIZATION_TEMPLATES(spt::sc::CompiledShader)

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
	return CanUseShadersCache()
		&& IsCachedShaderUpToDateImpl(CreateShaderFilePath(shaderRelativePath, shaderStageDef, compilationSettings), CreateShaderSourceCodeFilePath(shaderRelativePath));
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

} // spt::sc
