#include "TextureCompressor.h"
#include "RHICore/RHITextureTypes.h"

#define STB_DXT_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable: 4244)
#pragma warning(disable: 4083)
#include "stb_dxt.h"
#pragma warning pop


namespace spt::gfx::compressor
{

SizeType ComputeCompressedSizeBC1(math::Vector2u res)
{
	SPT_CHECK(res.x() % rhi::bc_info::bc1.blockWidth == 0 && res.y() % rhi::bc_info::bc1.blockHeight == 0);

	
	const Uint32 blocksX = res.x() / rhi::bc_info::bc1.blockWidth;
	const Uint32 blocksY = res.y() / rhi::bc_info::bc1.blockHeight;

	return static_cast<SizeType>(blocksX) * static_cast<SizeType>(blocksY) * rhi::bc_info::bc1.bytesPerBlock;
}


void CompressSurfaceToBC1(lib::Span<Byte> destBlock, const Surface2D& srcSurface, const CompressionParams& params /* = {} */)
{
	SPT_PROFILER_FUNCTION();

	static constexpr Uint32 inputBytesPerPixel = 4; // RGBA8

	SPT_CHECK(destBlock.size() == ComputeCompressedSizeBC1(srcSurface.res));
	SPT_CHECK(srcSurface.res.x() * srcSurface.res.y() * inputBytesPerPixel == srcSurface.data.size());

	const Uint32 blocksX = srcSurface.res.x() / rhi::bc_info::bc1.blockWidth;
	const Uint32 blocksY = srcSurface.res.y() / rhi::bc_info::bc1.blockHeight;

	int stbParams = 0;
	switch (params.quality)
	{
	case ECompressionQuality::Fast:
	case ECompressionQuality::Normal:
		stbParams = 0;
		break;
	case ECompressionQuality::High:
		stbParams = STB_DXT_HIGHQUAL;
		break;
	default:
		SPT_CHECK_NO_ENTRY();
	}

	const Uint32 srcRowBytes = srcSurface.res.x() * inputBytesPerPixel;

	Byte blockInputData[rhi::bc_info::bc1.blockWidth * rhi::bc_info::bc1.blockHeight * inputBytesPerPixel];

	for (Uint32 by = 0; by < blocksY; ++by)
	{
		for (Uint32 bx = 0; bx < blocksX; ++bx)
		{
			for (Uint32 row = 0; row < rhi::bc_info::bc1.blockHeight; ++row)
			{
				const Uint32 srcOffset = ((by * rhi::bc_info::bc1.blockHeight) + row) * srcRowBytes + (bx * rhi::bc_info::bc1.blockWidth * inputBytesPerPixel);
				const Uint32 destOffset = row * rhi::bc_info::bc1.blockWidth * inputBytesPerPixel;

				std::memcpy(&blockInputData[destOffset], &srcSurface.data[srcOffset], rhi::bc_info::bc1.blockWidth * inputBytesPerPixel);
			}

			const Uint32 destOffset = (by * blocksX + bx) * rhi::bc_info::bc1.bytesPerBlock;

			lib::Span<Byte> destBlockData = destBlock.subspan(destOffset, rhi::bc_info::bc1.bytesPerBlock);

			stb_compress_dxt_block(reinterpret_cast<unsigned char*>(destBlockData.data()),
								   reinterpret_cast<const unsigned char*>(blockInputData),
								   0, // no alpha
								   stbParams);
		}
	}
}

SizeType ComputeCompressedSizeBC4(math::Vector2u res)
{
	SPT_CHECK(res.x() % rhi::bc_info::bc4.blockWidth == 0 && res.y() % rhi::bc_info::bc4.blockHeight == 0);

	const Uint32 blocksX = res.x() / rhi::bc_info::bc4.blockWidth;
	const Uint32 blocksY = res.y() / rhi::bc_info::bc4.blockHeight;

	return static_cast<SizeType>(blocksX) * static_cast<SizeType>(blocksY) * rhi::bc_info::bc4.bytesPerBlock;
}

void CompressSurfaceToBC4(lib::Span<Byte> destBlock, const Surface2D& srcSurface, const CompressionParams& params /* = {} */)
{
	SPT_PROFILER_FUNCTION();

	static constexpr Uint32 inputBytesPerPixel = 1; // R8

	SPT_CHECK(destBlock.size() == ComputeCompressedSizeBC4(srcSurface.res));
	SPT_CHECK(srcSurface.res.x() * srcSurface.res.y() * inputBytesPerPixel == srcSurface.data.size());

	const Uint32 blocksX = srcSurface.res.x() / rhi::bc_info::bc4.blockWidth;
	const Uint32 blocksY = srcSurface.res.y() / rhi::bc_info::bc4.blockHeight;

	const Uint32 srcRowBytes = srcSurface.res.x() * inputBytesPerPixel;

	Byte blockInputData[rhi::bc_info::bc4.blockWidth * rhi::bc_info::bc4.blockHeight * inputBytesPerPixel];

	for (Uint32 by = 0; by < blocksY; ++by)
	{
		for (Uint32 bx = 0; bx < blocksX; ++bx)
		{
			for (Uint32 row = 0; row < rhi::bc_info::bc4.blockHeight; ++row)
			{
				const Uint32 srcOffset = ((by * rhi::bc_info::bc4.blockHeight) + row) * srcRowBytes + (bx * rhi::bc_info::bc4.blockWidth * inputBytesPerPixel);
				const Uint32 destOffset = row * rhi::bc_info::bc4.blockWidth * inputBytesPerPixel;

				std::memcpy(&blockInputData[destOffset], &srcSurface.data[srcOffset], rhi::bc_info::bc4.blockWidth * inputBytesPerPixel);
			}

			const Uint32 destOffset = (by * blocksX + bx) * rhi::bc_info::bc4.bytesPerBlock;

			lib::Span<Byte> destBlockData = destBlock.subspan(destOffset, rhi::bc_info::bc4.bytesPerBlock);

			stb_compress_bc4_block(reinterpret_cast<unsigned char*>(&destBlock[destOffset]), reinterpret_cast<const unsigned char*>(&blockInputData[0]));
		}
	}
}

SizeType ComputeCompressedSizeBC5(math::Vector2u res)
{
	SPT_CHECK(res.x() % rhi::bc_info::bc5.blockWidth == 0 && res.y() % rhi::bc_info::bc5.blockHeight == 0);

	const Uint32 blocksX = res.x() / rhi::bc_info::bc5.blockWidth;
	const Uint32 blocksY = res.y() / rhi::bc_info::bc5.blockHeight;

	return static_cast<SizeType>(blocksX) * static_cast<SizeType>(blocksY) * rhi::bc_info::bc5.bytesPerBlock;
}

void CompressSurfaceToBC5(lib::Span<Byte> destBlock, const Surface2D& srcSurface, const CompressionParams& params /* = {} */)
{
	SPT_PROFILER_FUNCTION();

	static constexpr Uint32 inputBytesPerPixel = 2; // R8

	SPT_CHECK(destBlock.size() == ComputeCompressedSizeBC5(srcSurface.res));
	SPT_CHECK(srcSurface.res.x() * srcSurface.res.y() * inputBytesPerPixel == srcSurface.data.size());

	const Uint32 blocksX = srcSurface.res.x() / rhi::bc_info::bc5.blockWidth;
	const Uint32 blocksY = srcSurface.res.y() / rhi::bc_info::bc5.blockHeight;

	const Uint32 srcRowBytes = srcSurface.res.x() * inputBytesPerPixel;

	Byte blockInputData[rhi::bc_info::bc5.blockWidth * rhi::bc_info::bc5.blockHeight * inputBytesPerPixel];

	for (Uint32 by = 0; by < blocksY; ++by)
	{
		for (Uint32 bx = 0; bx < blocksX; ++bx)
		{
			for (Uint32 row = 0; row < rhi::bc_info::bc5.blockHeight; ++row)
			{
				const Uint32 srcOffset = ((by * rhi::bc_info::bc5.blockHeight) + row) * srcRowBytes + (bx * rhi::bc_info::bc5.blockWidth * inputBytesPerPixel);
				const Uint32 destOffset = row * rhi::bc_info::bc5.blockWidth * inputBytesPerPixel;

				std::memcpy(&blockInputData[destOffset], &srcSurface.data[srcOffset], rhi::bc_info::bc5.blockWidth * inputBytesPerPixel);
			}

			const Uint32 destOffset = (by * blocksX + bx) * rhi::bc_info::bc5.bytesPerBlock;

			lib::Span<Byte> destBlockData = destBlock.subspan(destOffset, rhi::bc_info::bc5.bytesPerBlock);

			stb_compress_bc5_block(reinterpret_cast<unsigned char*>(&destBlock[destOffset]), reinterpret_cast<const unsigned char*>(&blockInputData[0]));
		}
	}
}

} // spt::gfx::compressor
