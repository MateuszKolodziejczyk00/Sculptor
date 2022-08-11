#include "CompiledShadersCache.h"
#include "ShaderCompilationEnvironment.h"
#include "ShaderCompilationInput.h"

#include <fstream>
#include <filesystem>

namespace spt::sc
{

Bool CompiledShadersCache::HasCachedShader(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings)
{
	SPT_PROFILE_FUNCTION();

	if (!CanUseShadersCache())
	{
		return false;
	}

	const lib::String filePath = CreateShaderFilePath(sourceCode, compilationSettings);
	return std::filesystem::exists(filePath);
}

CompiledShader CompiledShadersCache::TryGetCachedShader(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings)
{
	SPT_PROFILE_FUNCTION();
	
	SPT_CHECK(CanUseShadersCache());

	CompiledShader compiledShader;

	const lib::String filePath = CreateShaderFilePath(sourceCode, compilationSettings);

	std::ifstream stream(filePath, std::ios::binary | std::ios::ate); // place custom at the end of file, so that we can read how long the file is
	if (!stream.fail())
	{
		const SizeType binarySize = static_cast<SizeType>(stream.tellg());
		stream.seekg(0, std::ios::beg);

		lib::DynamicArray<Uint32> binary(binarySize / 4);

		if (stream.read(reinterpret_cast<char*>(binary.data()), binarySize))
		{
			compiledShader.SetBinary(std::move(binary));
		}

		stream.close();
	}

	return compiledShader;
}

CompiledShader CompiledShadersCache::GetCachedShaderChecked(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings)
{
	SPT_CHECK(HasCachedShader(sourceCode, compilationSettings));

	return TryGetCachedShader(sourceCode, compilationSettings);
}

void CompiledShadersCache::CacheShader(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings, const CompiledShader& shader)
{
	SPT_PROFILE_FUNCTION();
	
	SPT_CHECK(CanUseShadersCache());
	SPT_CHECK(shader.IsValid());

	const CompiledShader::Binary& binary = shader.GetBinary();

	const SizeType binarySizeInBytes = binary.size() * sizeof(CompiledShader::Binary::value_type);

	const lib::String filePath = CreateShaderFilePath(sourceCode, compilationSettings);
	std::ofstream stream(filePath, std::ios::trunc);
	stream.write(reinterpret_cast<const char*>(binary.data()), binarySizeInBytes);

	stream.close();
}

Bool CompiledShadersCache::CanUseShadersCache()
{
	return ShaderCompilationEnvironment::ShouldUseCompiledShadersCache()
		&& std::filesystem::is_directory(ShaderCompilationEnvironment::GetShadersCachePath());
}

CompiledShadersCache::HashType CompiledShadersCache::HashShader(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings)
{
	SPT_PROFILE_FUNCTION();

	return sourceCode.Hash() ^ compilationSettings.Hash();
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

lib::String CompiledShadersCache::CreateShaderFilePath(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings)
{
	SPT_PROFILE_FUNCTION();
	
	const HashType hash = HashShader(sourceCode, compilationSettings);
	return CreateShaderFilePath(hash);
}

}
