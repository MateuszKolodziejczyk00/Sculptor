#pragma once

#include "GraphicsMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::gfx
{

namespace compressor
{

struct Surface2D
{
	math::Vector2u res = math::Vector2u::Zero();
	lib::Span<const Byte> data;
};


enum class ECompressionQuality
{
	Fast,
	Normal,
	High
};


struct CompressionParams
{
	ECompressionQuality quality = ECompressionQuality::Normal;
};


SizeType GRAPHICS_API ComputeCompressedSizeBC1(math::Vector2u res);
void GRAPHICS_API CompressSurfaceToBC1(lib::Span<Byte> destBlock, const Surface2D& srcSurface, const CompressionParams& params = {});

SizeType GRAPHICS_API ComputeCompressedSizeBC4(math::Vector2u res);
void GRAPHICS_API CompressSurfaceToBC4(lib::Span<Byte> destBlock, const Surface2D& srcSurface, const CompressionParams& params = {});

SizeType GRAPHICS_API ComputeCompressedSizeBC5(math::Vector2u res);
void GRAPHICS_API CompressSurfaceToBC5(lib::Span<Byte> destBlock, const Surface2D& srcSurface, const CompressionParams& params = {});

} // compressor

} // spt::gfx
