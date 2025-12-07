#pragma once

#include "TextureAssetMacros.h"
#include "SculptorCoreTypes.h"
#include "TextureAsset.h"


namespace spt::as
{

struct TextureCompilationResult
{
	CompiledTexture         compiledTexture;
	lib::DynamicArray<Byte> textureData;
};


class TextureCompiler
{
public:

	TextureCompiler() = default;

	std::optional<TextureCompilationResult> CompileTexture(const TextureAsset& owningAsset, const TextureSourceDefinition& textureSource);

};

} // spt::as
