#pragma once

#include "MaterialsMacros.h"
#include "SculptorCoreTypes.h"
#include "SculptorCoreTypes.h"
#include "ECSRegistry.h"
#include "Pipelines/PSOsLibraryTypes.h"

namespace spt::sc
{
class ShaderCompilationSettings;
} // spt::sc


namespace spt::mat
{

struct MaterialShader
{
	Bool IsValidMaterial() const
	{
		return materialDataStructName.IsValid() && !!materialShaderHandle;
	}

	Bool operator==(const MaterialShader& other) const
	{
		return materialDataStructName == other.materialDataStructName
			&& materialShaderHandle == other.materialShaderHandle;
	}

	lib::HashedString materialDataStructName;
	ecs::EntityHandle materialShaderHandle;
};


MATERIALS_API void FillMaterialShaderCompilationSettings(const mat::MaterialShader& permutationDef, sc::ShaderCompilationSettings& outSettings);

} // spt::mat


namespace spt::rdr
{

namespace shader_translator
{

template<>
struct StructTranslator<mat::MaterialShader>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return StructTranslator<Uint32>::GetHLSLStructName();
	}
};

template<>
struct StructCPPToHLSLTranslator<mat::MaterialShader>
{
	static void Copy(const mat::MaterialShader& cppData, lib::Span<Byte> hlslData)
	{
		// Doesn't matter, Materials Hash is used only for permutations identification, not as shaders input
		StructCPPToHLSLTranslator<Uint32>::Copy(0u, hlslData);
	}
};

template<>
struct StructHLSLSizeEvaluator<mat::MaterialShader>
{
	static constexpr Uint32 Size()
	{
		return StructHLSLSizeEvaluator<Uint32>::Size();
	}
};

template<>
struct StructHLSLAlignmentEvaluator<mat::MaterialShader>
{
	static constexpr Uint32 Alignment()
	{
		return StructHLSLAlignmentEvaluator<Uint32>::Alignment();
	}
};


} // shader_translator

namespace permutations
{

template<>
struct ShaderCompilationSettingsBuilder<mat::MaterialShader>
{
	static void Build(const lib::String& variableName, const mat::MaterialShader& value, sc::ShaderCompilationSettings& outSettings)
	{
		mat::FillMaterialShaderCompilationSettings(value, outSettings);
	}
};

} // permutations

} // spt::rdr


namespace std
{

template<>
struct hash<spt::mat::MaterialShader>
{
	size_t operator()(const spt::mat::MaterialShader& permutation) const
	{
		return spt::lib::HashCombine(static_cast<size_t>(permutation.materialShaderHandle.entity()),
									 permutation.materialDataStructName);
	}
};

} // std
