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

	Bool						IsValid() const;
	
	void						SetSourceCode(lib::String&& code);
	void						SetShaderStage(rhi::EShaderStage stage);

	const lib::String&			GetSourceCode() const;
	rhi::EShaderStage			GetShaderStage() const;

	const char*					GetSourcePtr() const;
	SizeType					GetSourceLength() const;

	lib::String&				GetSourceCodeMutable();

	SizeType					Hash() const;

private:

	lib::String					m_code;

	rhi::EShaderStage			m_stage;
};


class SHADER_COMPILER_API ShaderCompilationSettings
{
public:

	ShaderCompilationSettings();

	void										AddMacroDefinition(MacroDefinition macro);

	const lib::DynamicArray<lib::HashedString>&	GetMacros() const;

	SizeType									Hash() const;

private:

	lib::DynamicArray<lib::HashedString>		m_macros;
};

}