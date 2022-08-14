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
	void					SetType(ECompiledShaderType type);
	
	const Binary&			GetBinary() const;
	ECompiledShaderType		GetType() const;

private:

	Binary					m_binary;
	ECompiledShaderType		m_type;
};


struct CompiledShadersGroup
{
public:

	lib::HashedString					m_groupName;
	lib::DynamicArray<CompiledShader>	m_shaders;
};


struct CompiledShaderFile
{
public:

	lib::DynamicArray<CompiledShadersGroup> m_groups;
};

} // spt::sc
