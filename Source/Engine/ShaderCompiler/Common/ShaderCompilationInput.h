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

	ShaderSourceCode(lib::String code, rhi::EShaderStage stage);

	Bool						IsValid() const;
	
	void						SetSourceCode(lib::String&& code);
	void						SetShaderStage(rhi::EShaderStage stage);

	const lib::String&			GetSourceCode() const;
	lib::String&				GetSourceCodeMutable();

	const char*					GetSourcePtr() const;
	SizeType					GetSourceLength() const;

	rhi::EShaderStage			GetShaderStage() const;

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


enum class EShaderCompilationFlags
{
	None			= 0,

	// Compiles shader only if source code was updated. If Cached shader is up to date compilation will return invalid shader
	UpdateOnly		= BIT(0),

	Default			= None
};

}