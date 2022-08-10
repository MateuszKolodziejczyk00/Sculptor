#pragma once

#include "ShaderCompilerMacros.h"
#include "ShaderCompilerTypes.h"
#include "SculptorCoreTypes.h"


namespace spt::sc
{

class SHADER_COMPILER_API ShaderSourceCode
{
public:

	explicit ShaderSourceCode(lib::HashedString name);
	
	void						SetSourceCode(lib::String&& code);
	void						SetShaderType(ECompiledShaderType type);

	lib::HashedString			GetName() const;
	const lib::String&			GetSourceCode() const;
	ECompiledShaderType			GetType() const;

	const char*					GetSourcePtr() const;
	SizeType					GetSourceLength() const;

private:

	lib::HashedString			m_name;

	lib::String					m_code;

	ECompiledShaderType			m_type;
};


class SHADER_COMPILER_API ShaderCompilationSettings
{
public:

	ShaderCompilationSettings();

	void										AddMacroDefinition(MacroDefinition macro);

	const lib::DynamicArray<MacroDefinition>&	GetMacros() const;

private:

	lib::DynamicArray<MacroDefinition>			m_macros;
};

}