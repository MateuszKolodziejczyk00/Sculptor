#include "CompiledShadersCache.h"
#include "Common/ShaderCompilationEnvironment.h"
#include "Common/ShaderCompilationInput.h"

#include "SculptorYAML.h"

#include <fstream>
#include <filesystem>

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

		for (const YAML::Node serializedShadersGroup : serializedShadersFile)
		{
			const lib::HashedString groupName = serializedShadersGroup["GroupName"].as<lib::HashedString>();

			const YAML::Node serializedShaders = serializedShadersGroup["Shaders"];

			CompiledShadersGroup& shadersGroup = compiledShaderFile.m_groups.emplace_back(CompiledShadersGroup());
			shadersGroup.m_groupName = groupName;

			for (const YAML::Node serializedShader : serializedShaders)
			{
				const rhi::EShaderStage shaderStage = static_cast<rhi::EShaderStage>(serializedShader["Type"].as<Uint32>());
				
				YAML::Binary deserializedBinary = serializedShader["Binary"].as<YAML::Binary>();
				lib::DynamicArray<unsigned char> binaryBytes;
				deserializedBinary.swap(binaryBytes);
				const SizeType binarySize = binaryBytes.size();
				SPT_CHECK(binarySize % sizeof(Uint32) == 0);

				lib::DynamicArray<Uint32> binary(binarySize / sizeof(Uint32));
				memcpy_s(binary.data(), binarySize, binaryBytes.data(), binarySize);

				CompiledShader& shader = shadersGroup.m_shaders.emplace_back(CompiledShader());
				shader.SetStage(shaderStage);
				shader.SetBinary(std::move(binary));
			}
		}

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
	out << YAML::BeginSeq; // groups sequence

	for (const CompiledShadersGroup& shadersGroup : shaderFile.m_groups)
	{
		out << YAML::BeginMap; // group elements map

		out << YAML::Key << "GroupName" << YAML::Value << shadersGroup.m_groupName;

		out << YAML::Key << "Shaders" << YAML::Value;
		out << YAML::BeginSeq; // shaders sequence

		for (const CompiledShader& shader : shadersGroup.m_shaders)
		{
			const CompiledShader::Binary& shaderCompiledBinary = shader.GetBinary();

			const YAML::Binary binary(reinterpret_cast<const unsigned char*>(shaderCompiledBinary.data()), shaderCompiledBinary.size() * sizeof(Uint32));

			out << YAML::BeginMap; // shader elements map

			out << YAML::Key << "Type" << YAML::Value << static_cast<Uint32>(shader.GetStage());
			out << YAML::Key << "Binary" << YAML::Value << binary;

			out << YAML::EndMap; // shader elements map
		}

		out << YAML::EndSeq; // shaders sequence

		out << YAML::EndMap; // group elements map
	}
	
	out << YAML::EndSeq; // groups sequence

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
