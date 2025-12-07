#pragma once

#include "TextureAssetMacros.h"
#include "SculptorCoreTypes.h"
#include "TextureSetAsset.h"
#include "DDC.h"


namespace spt::as
{

struct PBRCompiledTextureSetData
{
	CompiledPBRTextureSet compiledSet;

	lib::DynamicArray<Byte> texturesData;
};


class PBRTextureSetCompiler
{
public:

	PBRTextureSetCompiler() = default;

	std::optional<PBRCompiledTextureSetData> CompileTextureSet(const AssetHandle& owningAsset, const PBRTextureSetSource& source);

};

} // spt::as