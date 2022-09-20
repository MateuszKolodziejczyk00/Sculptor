#include "CompiledShadersCache.h"
#include "Common/ShaderCompilationEnvironment.h"
#include "Common/ShaderCompilationInput.h"
#include "FileSystem/File.h"
#include "Utility/String/StringUtils.h"

#include "SculptorYAML.h"
#include "SerializationHelper.h"
#include "ShaderMetaDataTypesSerialization.h"

namespace spt::srl
{

// spt::sc::CompiledShader ==============================================================

template<>
struct TypeSerializer<sc::CompiledShader>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		using ShaderBinary			= typename Param::Binary;
		using ShaderBinaryElement	= typename ShaderBinary::value_type;

		if constexpr (Serializer::IsSaving())
		{
			const srl::Binary binary(reinterpret_cast<const unsigned char*>(data.GetBinary().data()), data.GetBinary().size() * sizeof(ShaderBinaryElement));
			serializer.Serialize("Binary", binary);

			serializer.SerializeEnum("Stage", data.GetStage());
		}
		else
		{
			srl::Binary binary;
			serializer.Serialize("Binary", binary);

			SPT_CHECK(binary.size() % sizeof(ShaderBinaryElement) == 0);

			ShaderBinary shaderBinary(binary.size() / sizeof(ShaderBinaryElement));
			memcpy_s(shaderBinary.data(), binary.size(), binary.data(), binary.size());

			data.SetBinary(shaderBinary);

			rhi::EShaderStage stage = rhi::EShaderStage::None;
			serializer.SerializeEnum("Stage", stage);
			data.SetStage(stage);
		}
	}
};

// spt::sc::CompiledShaderFile ==========================================================

template<>
struct TypeSerializer<sc::CompiledShaderFile>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("Shaders", data.shaders);
		serializer.Serialize("MetaData", data.metaData);
	}
};

}

SPT_YAML_SERIALIZATION_TEMPLATES(spt::sc::CompiledShader)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::sc::CompiledShaderFile)

namespace spt::sc
{

Bool CompiledShadersCache::HasCachedShader(lib::HashedString shaderRelativePath, const ShaderCompilationSettings& compilationSettings)
{
	SPT_PROFILER_FUNCTION();

	if (!CanUseShadersCache())
	{
		return false;
	}

	const lib::String filePath = CreateShaderFilePath(shaderRelativePath, compilationSettings);
	return lib::File::Exists(filePath);
}

CompiledShaderFile CompiledShadersCache::TryGetCachedShader(lib::HashedString shaderRelativePath, const ShaderCompilationSettings& compilationSettings)
{
	SPT_PROFILER_FUNCTION();
	
	SPT_CHECK(CanUseShadersCache());

	CompiledShaderFile compiledShaderFile;

	const lib::String filePath = CreateShaderFilePath(shaderRelativePath, compilationSettings);
	srl::SerializationHelper::LoadTextStructFromFile(compiledShaderFile, filePath);

	return compiledShaderFile;
}

CompiledShaderFile CompiledShadersCache::GetCachedShaderChecked(lib::HashedString shaderRelativePath, const ShaderCompilationSettings& compilationSettings)
{
	SPT_CHECK(HasCachedShader(shaderRelativePath, compilationSettings));

	return TryGetCachedShader(shaderRelativePath, compilationSettings);
}

void CompiledShadersCache::CacheShader(lib::HashedString shaderRelativePath, const ShaderCompilationSettings& compilationSettings, const CompiledShaderFile& shaderFile)
{
	SPT_PROFILER_FUNCTION();
	
	SPT_CHECK(CanUseShadersCache());

	const lib::String filePath = CreateShaderFilePath(shaderRelativePath, compilationSettings);
	srl::SerializationHelper::SaveTextStructToFile(shaderFile, filePath);
}

Bool CompiledShadersCache::CanUseShadersCache()
{
	return ShaderCompilationEnvironment::ShouldUseCompiledShadersCache();
}

CompiledShadersCache::HashType CompiledShadersCache::HashShader(lib::HashedString shaderRelativePath, const ShaderCompilationSettings& compilationSettings)
{
	SPT_PROFILER_FUNCTION();

	return shaderRelativePath.GetKey() ^ compilationSettings.Hash();
}

lib::String CompiledShadersCache::CreateShaderFileName(HashType hash)
{
	return lib::StringUtils::ToHexString(reinterpret_cast<const Byte*>(&hash), sizeof(HashType));
}

lib::String CompiledShadersCache::CreateShaderFilePath(HashType hash)
{
	const lib::String shaderName = CreateShaderFileName(hash);
	return ShaderCompilationEnvironment::GetShadersCachePath() + "/" + shaderName;
}

lib::String CompiledShadersCache::CreateShaderFilePath(lib::HashedString shaderRelativePath, const ShaderCompilationSettings& compilationSettings)
{
	SPT_PROFILER_FUNCTION();
	
	const HashType hash = HashShader(shaderRelativePath, compilationSettings);
	return CreateShaderFilePath(hash);
}

}
