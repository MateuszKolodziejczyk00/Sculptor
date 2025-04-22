#pragma once

#include "TextureAssetMacros.h"
#include "SculptorCoreTypes.h"
#include "TextureSetAsset.h"
#include "DDC.h"


namespace spt::as
{

struct PBRTextureSetCompilationParams
{
	lib::SharedPtr<rdr::TextureView> baseColor;
	lib::SharedPtr<rdr::TextureView> metallicRoughness;
	lib::SharedPtr<rdr::TextureView> normals;
};


class PBRTextureSetCompiler
{
public:

	PBRTextureSetCompiler() = default;

	PBRCompiledTextureSetData CompileTextureSet(const PBRTextureSetCompilationParams& params, const DDC& ddc);

};

} // spt::as