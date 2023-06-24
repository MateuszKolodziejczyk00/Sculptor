#pragma once

#include "ShaderCompilerMacros.h"
#include "SculptorCoreTypes.h"
#include "ShaderCompilerTypes.h"
#include "ShaderMetaData.h"


namespace spt::sc
{

class SHADER_COMPILER_API CompiledShader
{
public:

	using Binary = lib::DynamicArray<Uint32>;

	CompiledShader();

	Bool IsValid() const;

	Binary					binary;
	rhi::EShaderStage		stage;
	lib::HashedString		entryPoint;
	smd::ShaderMetaData		metaData;
};

} // spt::sc
