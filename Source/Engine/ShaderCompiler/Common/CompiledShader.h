#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderCompilerTypes.h"


namespace spt::sc
{

struct CompiledShader
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


struct CompiledShadersGroup
{
public:

	CompiledShadersGroup();

	Bool								IsValid() const;

	lib::HashedString					m_groupName;
	lib::DynamicArray<CompiledShader>	m_shaders;
};


struct CompiledShaderFile
{
public:

	CompiledShaderFile();

	Bool									IsValid() const;

	lib::DynamicArray<CompiledShadersGroup> m_groups;
};

} // spt::sc
