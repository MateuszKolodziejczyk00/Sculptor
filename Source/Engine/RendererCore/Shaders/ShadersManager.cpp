#include "ShadersManager.h"
#include "Common/ShaderCompilerToolChain.h"
#include "Types/Shader.h"
#include "RendererUtils.h"

namespace spt::renderer
{

lib::SharedPtr<Shader> ShadersManager::GetShader(const lib::String& shaderRelativePath, const sc::ShaderCompilationSettings& settings, EShaderFlags flags /*= EShaderFlags::None*/)
{
	SPT_PROFILE_FUNCTION();

	const ShaderHashType shaderHash = HashCompilationParams(shaderRelativePath, settings);

	const auto foundShader = m_cachedShaders.find(shaderHash);
	if (foundShader != std::cend(m_cachedShaders))
	{
		return foundShader->second;
	}
	else
	{
		return CompileAndCacheShader(shaderRelativePath, settings, flags, shaderHash);
	}
}

ShadersManager::ShaderHashType ShadersManager::HashCompilationParams(const lib::String& shaderRelativePath, const sc::ShaderCompilationSettings& settings) const
{
	SPT_PROFILE_FUNCTION();

	ShadersManager::ShaderHashType hash = settings.Hash();
	lib::HashCombine(hash, lib::GetHash(shaderRelativePath));
	return hash;
}

lib::SharedPtr<Shader> ShadersManager::CompileAndCacheShader(const lib::String& shaderRelativePath, const sc::ShaderCompilationSettings& settings, EShaderFlags flags, ShaderHashType shaderHash)
{
	[[maybe_unused]]
	const lib::LockGuard<lib::Lock> lockGuard(m_lock);

	auto shaderIt = m_cachedShaders.find(shaderHash);
	if (shaderIt != std::cend(m_cachedShaders))
	{
		const lib::SharedPtr<Shader> shader = CompileShader(shaderRelativePath, settings, flags);

		shaderIt = m_cachedShaders.emplace(shaderHash, shader).first;
	}

	return shaderIt->second;
}

lib::SharedPtr<Shader> ShadersManager::CompileShader(const lib::String& shaderRelativePath, const sc::ShaderCompilationSettings& settings, EShaderFlags flags)
{
	SPT_PROFILE_FUNCTION();

	sc::CompiledShaderFile compiledShader;
	const Bool compilationResult = sc::ShaderCompilerToolChain::CompileShader(shaderRelativePath, settings, compiledShader);

	lib::SharedPtr<Shader> shader;
	if (compilationResult)
	{
		const lib::HashedString shaderName = shaderRelativePath;

		lib::DynamicArray<rhi::ShaderModuleDefinition> moduleDefinitions;
		moduleDefinitions.reserve(compiledShader.shaders.size());

		std::transform(std::cbegin(compiledShader.shaders), std::cend(compiledShader.shaders), std::back_inserter(moduleDefinitions),
			[](const sc::CompiledShader& shaderBinary)
			{
				return rhi::ShaderModuleDefinition(shaderBinary.GetBinary(), shaderBinary.GetStage());

			});

		shader = std::make_shared<Shader>(RENDERER_RESOURCE_NAME(shaderName), moduleDefinitions, compiledShader.metaData);
	}
	
	return shader;
}

} // spt::renderer
