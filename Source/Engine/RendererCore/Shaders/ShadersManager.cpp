#include "ShadersManager.h"
#include "Common/ShaderCompilerToolChain.h"
#include "Types/Shader.h"
#include "RendererUtils.h"
#include "RHIBridge/RHIImpl.h"
#include "ConfigUtils.h"
#include "JobSystem/JobSystem.h"
#include "Common/ShaderCompilationEnvironment.h"

#include "YAMLSerializerHelper.h"
#include "Renderer.h"
#include "Pipelines/PipelinesLibrary.h"

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
		serializer.Serialize("CompileWithDebugs", data.compileWithDebugs);
		serializer.Serialize("UseCompiledShadersCache", data.useCompiledShadersCache);
		serializer.Serialize("CacheSeparateSpvFile", data.cacheSeparateSpvFile);
		serializer.Serialize("ShadersPath", data.shadersPath);
		serializer.Serialize("ShadersCachePath", data.shadersCachePath);
		serializer.Serialize("ErrorLogsPath", data.errorLogsPath);
	}
};

} // spt::srl

SPT_YAML_SERIALIZATION_TEMPLATES(spt::sc::CompilationEnvironmentDef)


namespace spt::rdr
{

SPT_DEFINE_LOG_CATEGORY(ShadersManager, true)

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
	const Bool loaded = engn::ConfigUtils::LoadConfigData(compilationEnvironmentDef, "ShadersCompilationEnvironment.yaml");
	SPT_CHECK(loaded);
	compilationEnvironmentDef.targetEnvironment = priv::SelectCompilationTargetEnvironment(); // always override loaded target environment basing on current RHI

	sc::ShaderCompilationEnvironment::Initialize(compilationEnvironmentDef);
}

void ShadersManager::Uninitialize()
{
	ClearCachedShaders();
}

void ShadersManager::ClearCachedShaders()
{
	SPT_PROFILER_FUNCTION();

	const lib::WriteLockGuard lockGuard(m_lock);
	
	m_cachedShaders.clear();
}

ShaderID ShadersManager::CreateShader(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings, EShaderFlags flags /*= EShaderFlags::None*/)
{
	const ShaderHashType shaderHash = HashCompilationParams(shaderRelativePath, shaderStageDef, compilationSettings);

	Bool foundShader = false;

	{
		const lib::ReadLockGuard lockGuard(m_lock);

		const auto foundShaderIt = m_cachedShaders.find(shaderHash);
		if (foundShaderIt != std::cend(m_cachedShaders))
		{
			foundShader = true;
		}
	}

	if (!foundShader)
	{
		CompileAndCacheShader(shaderRelativePath, shaderStageDef, compilationSettings, flags, shaderHash);
	}

	return ShaderID(shaderHash, RENDERER_RESOURCE_NAME(shaderRelativePath));
}

lib::SharedRef<Shader> ShadersManager::GetShader(ShaderID shader) const
{
	SPT_PROFILER_FUNCTION();

	const lib::ReadLockGuard lockGuard(m_lock);

	return lib::Ref(m_cachedShaders.at(shader.GetID()));
}

#if WITH_SHADERS_HOT_RELOAD
void ShadersManager::HotReloadShaders()
{
	js::ParallelForEach(SPT_GENERIC_JOB_NAME,
						m_compiledShadersHotReloadParams,
						[this](ShaderHotReloadParameters& params)
						{
							const lib::SharedPtr<rdr::Shader> shader = CompileShader(params.shaderRelativePath, params.shaderStageDef, params.compilationSettings, sc::EShaderCompilationFlags::UpdateOnly, params.flags);
							if (shader)
							{
								SPT_LOG_INFO(ShadersManager, "Hot reloaded shader: {} ({})", params.shaderRelativePath.data(), params.shaderStageDef.entryPoint.GetData());

								{
									const lib::WriteLockGuard lockGuard(m_lock);
									m_cachedShaders[params.shaderHash] = shader;
								}
								
								const ShaderID shaderID(params.shaderHash, RENDERER_RESOURCE_NAME(params.shaderRelativePath));
								Renderer::GetPipelinesLibrary().InvalidatePipelinesUsingShader(shaderID);
							}
						},
						js::JobDef().SetPriority(js::EJobPriority::Low),
						js::JobDef().SetPriority(js::EJobPriority::Low));

}
#endif // WITH_SHADERS_HOT_RELOAD

ShaderHashType ShadersManager::HashCompilationParams(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings) const
{
	return lib::HashCombine(shaderStageDef, compilationSettings.Hash(), lib::GetHash(shaderRelativePath));
}

void ShadersManager::CompileAndCacheShader(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings, EShaderFlags flags, ShaderHashType shaderHash)
{
	SPT_PROFILER_FUNCTION();

	const lib::WriteLockGuard lockGuard(m_lock);

	// check once again if shader object is missing (it might have changed when we were waiting for acquiring lock)
	auto shaderIt = m_cachedShaders.find(shaderHash);
	if (shaderIt == std::cend(m_cachedShaders))
	{
		lib::SharedPtr<Shader> shader;
		while (!shader)
		{
			shader = CompileShader(shaderRelativePath, shaderStageDef, compilationSettings, sc::EShaderCompilationFlags::Default, flags);
			if (!shader)
			{
				SPT_LOG_ERROR(ShadersManager, "Failed to compile shader: {} ({}). Attempting again in 2 seconds...", shaderRelativePath.data(), shaderStageDef.entryPoint.GetData());

				std::this_thread::sleep_for(std::chrono::seconds(2));
			}
		}

		shaderIt = m_cachedShaders.emplace(shaderHash, shader).first;
		OnShaderCompiled(shaderRelativePath, shaderStageDef, compilationSettings, flags, shaderHash);
	}

	SPT_CHECK_MSG(shaderIt != std::cend(m_cachedShaders), "Failed to compile shader! {0}", shaderRelativePath.data());
}

lib::SharedPtr<Shader> ShadersManager::CompileShader(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings, sc::EShaderCompilationFlags compilationFlags, EShaderFlags flags)
{
	SPT_PROFILER_FUNCTION();

	sc::CompiledShader compilationResult = sc::ShaderCompilerToolChain::CompileShader(shaderRelativePath, shaderStageDef, compilationSettings, compilationFlags);

	lib::SharedPtr<Shader> shader;
	if (compilationResult.IsValid())
	{
		const lib::HashedString shaderName = shaderRelativePath + "_" + shaderStageDef.entryPoint.ToString();
		const rhi::ShaderModuleDefinition moduleDefinition(std::move(compilationResult.binary), compilationResult.stage, compilationResult.entryPoint);
		shader = lib::MakeShared<Shader>(RENDERER_RESOURCE_NAME(shaderName), moduleDefinition, std::move(compilationResult.metaData));
	}
	
	return shader;
}

void ShadersManager::OnShaderCompiled(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings, EShaderFlags flags, ShaderHashType shaderHash)
{
#if WITH_SHADERS_HOT_RELOAD
	CacheShaderHotReloadParams(shaderRelativePath, shaderStageDef, compilationSettings, flags, shaderHash);
#endif // WITH_SHADERS_HOT_RELOAD
}

#if WITH_SHADERS_HOT_RELOAD
void ShadersManager::CacheShaderHotReloadParams(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings, EShaderFlags flags, ShaderHashType shaderHash)
{
	SPT_PROFILER_FUNCTION();

	ShaderHotReloadParameters& hotReloadParams = m_compiledShadersHotReloadParams.emplace_back(ShaderHotReloadParameters{});
	hotReloadParams.shaderRelativePath	= shaderRelativePath;
	hotReloadParams.shaderStageDef		= shaderStageDef;
	hotReloadParams.compilationSettings	= compilationSettings;
	hotReloadParams.flags				= flags;
	hotReloadParams.shaderHash			= shaderHash;
}
#endif // WITH_SHADERS_HOT_RELOAD

} // spt::rdr
