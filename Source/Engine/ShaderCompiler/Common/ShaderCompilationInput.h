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


struct ShaderStageCompilationDef
{
	ShaderStageCompilationDef(rhi::EShaderStage inStage = rhi::EShaderStage::None, const lib::HashedString& inEntryPoint = "Main")
		: stage(inStage)
		, entryPoint(inEntryPoint)
	{ }

	SizeType Hash() const
	{
		return spt::lib::HashCombine(stage,
									 entryPoint);
	}

	Bool IsRayTracing() const
	{
		return stage == rhi::EShaderStage::RTAnyHit
			|| stage == rhi::EShaderStage::RTClosestHit
			|| stage == rhi::EShaderStage::RTGeneration
			|| stage == rhi::EShaderStage::RTIntersection
			|| stage == rhi::EShaderStage::RTMiss;
	}

	rhi::EShaderStage stage;
	lib::HashedString entryPoint;
};


class SHADER_COMPILER_API ShaderCompilationSettings
{
public:

	ShaderCompilationSettings();

	void AddMacroDefinition(MacroDefinition macro);
	const lib::DynamicArray<lib::HashedString>& GetMacros() const;

	void DisableGeneratingDebugSource();
	Bool ShouldGenerateDebugSource() const;

	SizeType Hash() const;

private:

	lib::DynamicArray<lib::HashedString> m_macros;
	
	Bool m_generateDebugSource;
};


enum class EShaderCompilationFlags
{
	None			= 0,

	// Compiles shader only if source code was updated. If Cached shader is up to date compilation will return invalid shader
	UpdateOnly		= BIT(0),

	Default			= None
};

} // spt::sc


namespace spt::lib
{

template<>
struct Hasher<sc::ShaderStageCompilationDef>
{
    size_t operator()(const sc::ShaderStageCompilationDef& stageCompilation) const
    {
		return stageCompilation.Hash();
    }
};

} // spt::lib
