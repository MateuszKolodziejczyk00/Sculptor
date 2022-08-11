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

	const HashType hash = HashShader(sourceCode, compilationSettings);
	const lib::String fileName = CreateShaderFileName(hash);
	return std::filesystem::exists(fileName);
}

Bool CompiledShadersCache::CanUseShadersCache()
{
	return ShaderCompilationEnvironment::ShouldUseCompiledShadersCache()
		&& std::filesystem::is_directory(ShaderCompilationEnvironment::GetShadersCachePath());
}

CompiledShadersCache::HashType CompiledShadersCache::HashShader(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings)
{
	SPT_PROFILE_FUNCTION();
	
	SPT_CHECK(CanUseShadersCache());

	return sourceCode.Hash() ^ compilationSettings.Hash();
}

lib::String CompiledShadersCache::CreateShaderFileName(HashType hash)
{
	constexpr Uint32 fileNameLength = sizeof(HashType);

	lib::String result(fileNameLength, '\0');
	memcpy_s(result.data(), fileNameLength, &hash, fileNameLength);

	return result;
}

}
