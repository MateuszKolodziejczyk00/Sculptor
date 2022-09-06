#include "ShadersManager.h"
#include "Common/ShaderCompilerToolChain.h"
#include "Types/Shader.h"
#include "RendererUtils.h"
#include "RHIBridge/RHIImpl.h"
#include "Engine.h"
#include "Common/ShaderCompilationEnvironment.h"

#include "YAMLSerializerHelper.h"

namespace spt::srl
{

// spt::sc::CompilationEnvironmentDef ===================================================

template<>
struct TypeSerializer<sc::CompilationEnvironmentDef>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.SerializeEnum("Environment", data.targetEnvironment);
		serializer.Serialize("GenerateDebugInfo", data.generateDebugInfo);
		serializer.Serialize("UseCompiledShadersCache", data.useCompiledShadersCache);
		serializer.Serialize("ShadersPath", data.shadersPath);
		serializer.Serialize("ShadersCachePath", data.shadersCachePath);
		serializer.Serialize("ErrorLogsPath", data.errorLogsPath);
	}
};

} // spt::srl

SPT_YAML_SERIALIZATION_TEMPLATES(spt::sc::CompilationEnvironmentDef)


namespace spt::rdr
{

namespace priv
{

static sc::ETargetEnvironment SelectCompilationTargetEnvironment()
{
	const rhi::ERHIType rhiType = rhi::RHI::GetRHIType();

	switch (rhiType)
	{
	case rhi::ERHIType::Vulkan_1_3:		return sc::ETargetEnvironment::Vulkan_1_3;

	default:

		SPT_CHECK_NO_ENTRY();
		return sc::ETargetEnvironment::None;
	}
}

} // priv

void ShadersManager::Initialize()
{
	SPT_PROFILER_FUNCTION();
	
	sc::CompilationEnvironmentDef compilationEnvironmentDef;
	const Bool loaded = engn::Engine::LoadConfigData(compilationEnvironmentDef, "ShadersCompilationEnvironment.yaml");
	SPT_CHECK(loaded);
	compilationEnvironmentDef.targetEnvironment = priv::SelectCompilationTargetEnvironment(); // always override loaded target environment basing on current RHI

	sc::ShaderCompilationEnvironment::Initialize(compilationEnvironmentDef);
}

void ShadersManager::Uninitialize()
{
	ClearCachedShaders();
}

lib::SharedPtr<Shader> ShadersManager::GetShader(const lib::String& shaderRelativePath, const sc::ShaderCompilationSettings& settings, EShaderFlags flags /*= EShaderFlags::None*/)
{
	SPT_PROFILER_FUNCTION();

	const ShaderHashType shaderHash = HashCompilationParams(shaderRelativePath, settings);

	{
		SPT_MAYBE_UNUSED
		const lib::ReadLockGuard lockGuard(m_lock);

		const auto foundShader = m_cachedShaders.find(shaderHash);
		if (foundShader != std::cend(m_cachedShaders))
		{
			return foundShader->second;
		}
	}

	return CompileAndCacheShader(shaderRelativePath, settings, flags, shaderHash);
}

void ShadersManager::ClearCachedShaders()
{
	SPT_PROFILER_FUNCTION();

	SPT_MAYBE_UNUSED
	const lib::WriteLockGuard lockGuard(m_lock);
	
	m_cachedShaders.clear();
}

ShadersManager::ShaderHashType ShadersManager::HashCompilationParams(const lib::String& shaderRelativePath, const sc::ShaderCompilationSettings& settings) const
{
	SPT_PROFILER_FUNCTION();

	ShadersManager::ShaderHashType hash = settings.Hash();
	lib::HashCombine(hash, lib::GetHash(shaderRelativePath));
	return hash;
}

lib::SharedPtr<Shader> ShadersManager::CompileAndCacheShader(const lib::String& shaderRelativePath, const sc::ShaderCompilationSettings& settings, EShaderFlags flags, ShaderHashType shaderHash)
{
	SPT_PROFILER_FUNCTION();

	SPT_MAYBE_UNUSED
	const lib::WriteLockGuard lockGuard(m_lock);

	auto shaderIt = m_cachedShaders.find(shaderHash);
	if (shaderIt == std::cend(m_cachedShaders))
	{
		const lib::SharedPtr<Shader> shader = CompileShader(shaderRelativePath, settings, flags);
		if (shader)
		{
			shaderIt = m_cachedShaders.emplace(shaderHash, shader).first;
		}
	}

	return shaderIt != std::cend(m_cachedShaders) ? shaderIt->second : lib::SharedPtr<Shader>{};
}

lib::SharedPtr<Shader> ShadersManager::CompileShader(const lib::String& shaderRelativePath, const sc::ShaderCompilationSettings& settings, EShaderFlags flags)
{
	SPT_PROFILER_FUNCTION();

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

		shader = std::make_shared<Shader>(RENDERER_RESOURCE_NAME(shaderName), moduleDefinitions, std::make_shared<smd::ShaderMetaData>(compiledShader.metaData));
	}
	
	return shader;
}

} // spt::rdr
