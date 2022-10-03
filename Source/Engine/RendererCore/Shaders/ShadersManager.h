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

	SPT_NODISCARD ShaderID					CreateShader(const lib::String& shaderRelativePath, const sc::ShaderCompilationSettings& settings, EShaderFlags flags = EShaderFlags::None);
	SPT_NODISCARD lib::SharedRef<Shader>	GetShader(ShaderID shader) const;

private:

	ShaderHashType			HashCompilationParams(const lib::String& shaderRelativePath, const sc::ShaderCompilationSettings& settings) const;

	void					CompileAndCacheShader(const lib::String& shaderRelativePath, const sc::ShaderCompilationSettings& settings, EShaderFlags flags, ShaderHashType shaderHash);

	lib::SharedPtr<Shader>	CompileShader(const lib::String& shaderRelativePath, const sc::ShaderCompilationSettings& settings, EShaderFlags flags);

	lib::HashMap<ShaderHashType, lib::SharedPtr<Shader>> m_cachedShaders;

	mutable lib::ReadWriteLock m_lock;
};

} // spt::rdr
