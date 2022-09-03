#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "Common/ShaderCompilationInput.h"

namespace spt::rdr
{

class Shader;

enum class EShaderFlags : Flags32
{
	None			= 0
};

class RENDERER_CORE_API ShadersManager
{
public:

	ShadersManager() = default;

	void					Initialize();
	void					Uninitialize();

	lib::SharedPtr<Shader>	GetShader(const lib::String& shaderRelativePath, const sc::ShaderCompilationSettings& settings, EShaderFlags flags = EShaderFlags::None);

	void					ClearCachedShaders();

private:

	using ShaderHashType = SizeType;

	ShaderHashType			HashCompilationParams(const lib::String& shaderRelativePath, const sc::ShaderCompilationSettings& settings) const;

	lib::SharedPtr<Shader>	CompileAndCacheShader(const lib::String& shaderRelativePath, const sc::ShaderCompilationSettings& settings, EShaderFlags flags, ShaderHashType shaderHash);

	lib::SharedPtr<Shader>	CompileShader(const lib::String& shaderRelativePath, const sc::ShaderCompilationSettings& settings, EShaderFlags flags);

	lib::HashMap<ShaderHashType, lib::SharedPtr<Shader>> m_cachedShaders;

	mutable lib::ReadWriteLock m_lock;
};

} // spt::renderer
