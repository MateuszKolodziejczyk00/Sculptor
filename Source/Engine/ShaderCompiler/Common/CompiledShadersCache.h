#pragma once

#include "SculptorCoreTypes.h"


namespace spt::sc
{

class ShaderSourceCode;
class ShaderCompilationSettings;


class CompiledShadersCache
{
public:

	static Bool			HasCachedShader(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings);

private:

	using HashType		= Uint64;

	static Bool			CanUseShadersCache();

	static HashType		HashShader(const ShaderSourceCode& sourceCode, const ShaderCompilationSettings& compilationSettings);

	static lib::String	CreateShaderFileName(HashType hash);
};

}