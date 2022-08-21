#include "CompiledShadersCache.h"
#include "Common/ShaderCompilationEnvironment.h"
#include "Common/ShaderCompilationInput.h"

#include "SculptorYAML.h"
#include "ShaderMetaDataTypesSerialization.h"

#include <fstream>
#include <filesystem>

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

		if constexpr (Serializer::IsWriting())
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

// spt::sc::CompiledShadersGroup ========================================================

template<>
struct TypeSerializer<sc::CompiledShadersGroup>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("GroupName", data.groupName);
		serializer.Serialize("Shaders", data.shaders);
		serializer.Serialize("MetaData", data.metaData);
	}
};

// spt::sc::CompiledShaderFile ==========================================================

template<>
struct TypeSerializer<sc::CompiledShaderFile>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("ShaderGroups", data.groups);
	}
};

}

SPT_YAML_SERIALIZATION_TEMPLATES(spt::sc::CompiledShader)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::sc::CompiledShadersGroup)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::sc::CompiledShaderFile)

namespace spt::sc
{

Bool CompiledShadersCache::HasCachedShader(lib::HashedString shaderRelativePath, const ShaderCompilationSettings& compilationSettings)
{
	SPT_PROFILE_FUNCTION();

	if (!CanUseShadersCache())
	{
		return false;
	}

	const lib::String filePath = CreateShaderFilePath(shaderRelativePath, compilationSettings);
	return std::filesystem::exists(filePath);
}

CompiledShaderFile CompiledShadersCache::TryGetCachedShader(lib::HashedString shaderRelativePath, const ShaderCompilationSettings& compilationSettings)
{
	SPT_PROFILE_FUNCTION();
	
	SPT_CHECK(CanUseShadersCache());

	CompiledShaderFile compiledShaderFile;

	const lib::String filePath = CreateShaderFilePath(shaderRelativePath, compilationSettings);

	std::ifstream stream(filePath);
	if (!stream.fail())
	{
		std::stringstream stringStream;
		stringStream << stream.rdbuf();

		const YAML::Node serializedShadersFile = YAML::Load(stringStream.str());

		compiledShaderFile = serializedShadersFile["CompiledShaderFile"].as<CompiledShaderFile>();

		stream.close();
	}

	return compiledShaderFile;
}

CompiledShaderFile CompiledShadersCache::GetCachedShaderChecked(lib::HashedString shaderRelativePath, const ShaderCompilationSettings& compilationSettings)
{
	SPT_CHECK(HasCachedShader(shaderRelativePath, compilationSettings));

	return TryGetCachedShader(shaderRelativePath, compilationSettings);
}

void CompiledShadersCache::CacheShader(lib::HashedString shaderRelativePath, const ShaderCompilationSettings& compilationSettings, const CompiledShaderFile& shaderFile)
{
	SPT_PROFILE_FUNCTION();
	
	SPT_CHECK(CanUseShadersCache());

	YAML::Emitter out;
	out << YAML::BeginMap;

	out << YAML::Key << "CompiledShaderFile" << YAML::Value << shaderFile;
	
	out << YAML::BeginMap;

	const lib::String filePath = CreateShaderFilePath(shaderRelativePath, compilationSettings);
	std::ofstream stream(filePath, std::ios::trunc);
	stream << out.c_str();

	stream.close();
}

Bool CompiledShadersCache::CanUseShadersCache()
{
	return ShaderCompilationEnvironment::ShouldUseCompiledShadersCache()
		&& std::filesystem::is_directory(ShaderCompilationEnvironment::GetShadersCachePath());
}

CompiledShadersCache::HashType CompiledShadersCache::HashShader(lib::HashedString shaderRelativePath, const ShaderCompilationSettings& compilationSettings)
{
	SPT_PROFILE_FUNCTION();

	return shaderRelativePath.GetKey() ^ compilationSettings.Hash();
}

lib::String CompiledShadersCache::CreateShaderFileName(HashType hash)
{
	constexpr Uint32 fileNameLength = sizeof(HashType);

	lib::String result(fileNameLength, '\0');
	memcpy_s(result.data(), fileNameLength, &hash, fileNameLength);

	return result;
}

lib::String CompiledShadersCache::CreateShaderFilePath(HashType hash)
{
	const lib::String shaderName = CreateShaderFileName(hash);
	return ShaderCompilationEnvironment::GetShadersCachePath() + "/" + shaderName;
}

lib::String CompiledShadersCache::CreateShaderFilePath(lib::HashedString shaderRelativePath, const ShaderCompilationSettings& compilationSettings)
{
	SPT_PROFILE_FUNCTION();
	
	const HashType hash = HashShader(shaderRelativePath, compilationSettings);
	return CreateShaderFilePath(hash);
}

}
