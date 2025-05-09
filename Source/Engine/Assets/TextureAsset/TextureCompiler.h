#pragma once

#include "TextureAssetMacros.h"
#include "SculptorCoreTypes.h"
#include "TextureAsset.h"
#include "DDC.h"


namespace spt::as
{

class TextureCompiler
{
public:

	TextureCompiler() = default;

	CompiledTextureData CompileTexture(const TextureSourceDefinition& textureSource, const TextureAsset& owningAsset, const DDC& ddc);

};

} // spt::as
