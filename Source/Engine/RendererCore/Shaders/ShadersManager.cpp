#include "ShadersManager.h"
#include "Common/ShaderCompilerToolChain.h"
#include "Types/Shader.h"
#include "RendererUtils.h"
#include "RHIBridge/RHIImpl.h"
#include "ConfigUtils.h"
#include "JobSystem/JobSystem.h"
#include "Common/ShaderCompilationEnvironment.h"
#include "Pipelines/PipelinesCache.h"
#include "Renderer.h"


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
	const Bool loaded = engn::ConfigUtils::LoadConfigData(compilationEnvironmentDef, "ShadersCompilationEnvironment.json");
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

ShaderID ShadersManager::GenerateShaderID(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings) const
{
	const ShaderHashType shaderHash = HashCompilationParams(shaderRelativePath, shaderStageDef, compilationSettings);
	return ShaderID(shaderHash, RENDERER_RESOURCE_NAME(shaderRelativePath));
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

	while (true)
	{
		{
			const lib::ReadLockGuard lockGuard(m_lock);

			lib::SharedPtr<Shader> foundShader = m_cachedShaders.at(shader.GetID());

			if(foundShader)
			{
				return lib::Ref(foundShader);
			}
		}

		// If shader is not yet compiled, wait a bit and try again (this is super unlikely in practice)
		js::Worker::TryActiveWait();
	}
}

#if WITH_SHADERS_HOT_RELOAD
void ShadersManager::HotReloadShaders()
{
	struct ShaderHotReloadState
	{
		lib::DynamicArray<ShaderID> invalidatedShaderIDs;
		std::atomic<Uint32> invalidatedShadersNum = 0;
		std::atomic<Int32> pendingCompilationsNum = 0;
	};

	lib::SharedPtr<ShaderHotReloadState> hotReloadState = lib::MakeShared<ShaderHotReloadState>();
	hotReloadState->pendingCompilationsNum = static_cast<Int32>(m_compiledShadersHotReloadParams.size());
	hotReloadState->invalidatedShaderIDs.resize(m_compiledShadersHotReloadParams.size());

	js::ParallelForEach(SPT_GENERIC_JOB_NAME,
						m_compiledShadersHotReloadParams,
						[this, hotReloadState](ShaderHotReloadParameters& params)
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
								hotReloadState->invalidatedShaderIDs[hotReloadState->invalidatedShadersNum++] = shaderID;
							}

							if (hotReloadState->pendingCompilationsNum.fetch_sub(1) == 1)
							{
								SPT_LOG_INFO(ShadersManager, "Finished hot reloading shaders. Invalidating pipelines using {} shaders...", hotReloadState->invalidatedShadersNum.load());
								Renderer::GetPipelinesCache().InvalidatePipelinesUsingShaders(lib::Span<const ShaderID>(hotReloadState->invalidatedShaderIDs.data(), hotReloadState->invalidatedShadersNum.load()));
							}
						},
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

	bool thisThreadSHouldCompile = false;

	{
		const lib::WriteLockGuard lockGard(m_lock);

		auto [shaderIt, wasEmplaced] = m_cachedShaders.try_emplace(shaderHash);
		thisThreadSHouldCompile = wasEmplaced;
	}

	if (thisThreadSHouldCompile)
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

		OnShaderCompiled(shaderRelativePath, shaderStageDef, compilationSettings, flags, shaderHash);

		{
			const lib::WriteLockGuard lockGard(m_lock);
			m_cachedShaders[shaderHash] = shader;
		}
	}
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

	const lib::LockGuard lock(m_hotReloadParamsLock);

	ShaderHotReloadParameters& hotReloadParams = m_compiledShadersHotReloadParams.emplace_back(ShaderHotReloadParameters{});
	hotReloadParams.shaderRelativePath	= shaderRelativePath;
	hotReloadParams.shaderStageDef		= shaderStageDef;
	hotReloadParams.compilationSettings	= compilationSettings;
	hotReloadParams.flags				= flags;
	hotReloadParams.shaderHash			= shaderHash;
}
#endif // WITH_SHADERS_HOT_RELOAD

} // spt::rdr
