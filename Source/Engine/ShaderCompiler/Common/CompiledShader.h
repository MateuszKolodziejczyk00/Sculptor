#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderCompilerTypes.h"
#include "ShaderMetaData.h"


namespace spt::sc
{

class CompiledShader
{
public:

	using Binary = lib::DynamicArray<Uint32>;

	CompiledShader();

	Bool					IsValid() const;

	void					SetBinary(Binary binary);
	void					SetStage(rhi::EShaderStage stage);
	
	const Binary&			GetBinary() const;
	rhi::EShaderStage		GetStage() const;

private:

	Binary					m_binary;
	rhi::EShaderStage		m_stage;
};


struct CompiledShaderFile
{
public:

	CompiledShaderFile();

	Bool								IsValid() const;

	lib::DynamicArray<CompiledShader>	shaders;
	smd::ShaderMetaData					metaData;
};

} // spt::sc
