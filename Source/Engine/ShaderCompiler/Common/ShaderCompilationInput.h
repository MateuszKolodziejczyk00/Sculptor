#pragma once

#include "ShaderCompilerMacros.h"
#include "ShaderCompilerTypes.h"
#include "SculptorCoreTypes.h"


namespace spt::sc
{

class SHADER_COMPILER_API ShaderSourceCode
{
public:

	ShaderSourceCode();
	
	void						SetSourceCode(lib::String&& code);

	const lib::String&			GetSourceCode() const;

private:

	lib::String					m_code;
};


class SHADER_COMPILER_API ShaderCompilationSettings
{
public:

	ShaderCompilationSettings();

	void SetShaderType(ECompiledShaderType type);
	void AddMacroDefinition(MacroDefinition macro);

	const lib::DynamicArray<MacroDefinition>&	GetMacros() const;
	ECompiledShaderType							GetType() const;

private:

	lib::DynamicArray<MacroDefinition>		m_macros;

	ECompiledShaderType						m_type;
};

}