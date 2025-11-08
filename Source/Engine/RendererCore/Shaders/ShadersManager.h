#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "Common/ShaderCompilationInput.h"
#include "ShaderTypes.h"

namespace spt::rdr
{

class Shader;


class ShadersManager
{
public:

	ShadersManager() = default;

	void		Initialize();
	void		Uninitialize();

	void		ClearCachedShaders();

	SPT_NODISCARD ShaderID					GenerateShaderID(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings) const;

	SPT_NODISCARD ShaderID					CreateShader(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings, EShaderFlags flags = EShaderFlags::None);
	SPT_NODISCARD lib::SharedRef<Shader>	GetShader(ShaderID shader) const;

#if WITH_SHADERS_HOT_RELOAD
	void HotReloadShaders();
#endif // WITH_SHADERS_HOT_RELOAD

private:

	ShaderHashType			HashCompilationParams(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings) const;

	void					CompileAndCacheShader(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings, EShaderFlags flags, ShaderHashType shaderHash);

	lib::SharedPtr<Shader>	CompileShader(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings, sc::EShaderCompilationFlags compilationFlags, EShaderFlags flags);

	void					OnShaderCompiled(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings, EShaderFlags flags, ShaderHashType shaderHash);

#if WITH_SHADERS_HOT_RELOAD
	void CacheShaderHotReloadParams(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings, EShaderFlags flags, ShaderHashType shaderHash);
#endif // WITH_SHADERS_HOT_RELOAD

	lib::HashMap<ShaderHashType, lib::SharedPtr<Shader>> m_cachedShaders;

	mutable lib::ReadWriteLock m_lock;

#if WITH_SHADERS_HOT_RELOAD

	struct ShaderHotReloadParameters
	{
		ShaderHotReloadParameters()
			: flags(EShaderFlags::None)
			, shaderHash(idxNone<ShaderHashType>)
		{ }

		lib::String shaderRelativePath;
		sc::ShaderStageCompilationDef shaderStageDef;
		sc::ShaderCompilationSettings compilationSettings;
		EShaderFlags flags;
		ShaderHashType shaderHash;
	};

	mutable lib::ReadWriteLock m_hotReloadParamsLock;
	lib::DynamicArray<ShaderHotReloadParameters> m_compiledShadersHotReloadParams;

#endif // WITH_SHADERS_HOT_RELOAD
};

} // spt::rdr
