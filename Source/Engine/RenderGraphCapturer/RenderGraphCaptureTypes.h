#pragma once

#include "SculptorCoreTypes.h"

namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rg::capture
{

enum class ECapturedResourceFlags
{
	None = 0,
};


struct RGResourceCapture
{
	ECapturedResourceFlags flags;
};


struct RGTextureCapture : public RGResourceCapture
{
	lib::SharedPtr<rdr::TextureView> textureView;
};


struct RGNodeCapture
{
	lib::HashedString name;

	lib::DynamicArray<RGTextureCapture> outputTextures;
};


struct RGCapture
{
	lib::DynamicArray<RGNodeCapture> nodes;
};

} // spt::rg::capture
