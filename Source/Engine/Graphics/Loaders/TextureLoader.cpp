#include "TextureLoader.h"
#include "Types/Texture.h"
#include "ResourcesManager.h"
#include "Transfers/UploadsManager.h"
#include "RenderGraphBuilder.h"
#include "FileSystem/File.h"
#include "RHICore/SculptorDXGIFormats.h"

#pragma warning(push)
#pragma warning(disable: 4244)
#pragma warning(disable: 4996)

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#pragma warning(pop)

#include "dds.h"


SPT_DEFINE_LOG_CATEGORY(ImageLoader, true);
#pragma optimize("", off)


namespace spt::gfx
{

struct TextureDataView
{
public:

	virtual ~TextureDataView() = default;

	virtual lib::Span<const Byte> GetSurfaceData(Uint32 mipLevel, Uint32 arrayLayer) const = 0;

	rhi::EFragmentFormat format = rhi::EFragmentFormat::None;

	math::Vector3u resolution = {};

	Uint32 mipLevelsNum = 0u;
	Uint32 arrayLayersNum = 0u;
};

namespace png_jpg
{

struct STBTextureDataView : public TextureDataView
{
	// Begin TextureDataView overrides
	virtual lib::Span<const Byte> GetSurfaceData(Uint32 mipLevel, Uint32 arrayLayer) const override
	{
		SPT_CHECK(!!imageData);
		SPT_CHECK(mipLevel == 0u);
		SPT_CHECK(arrayLayer == 0u);

		return { reinterpret_cast<const Byte*>(imageData), imageDataSize };
	}
	// End TextureDataView overrides

	stbi_uc* imageData   = nullptr;
	Uint64 imageDataSize = 0u;
};


template<typename TCallback>
Bool LoadTextureImpl(TCallback&& callback, lib::StringView path)
{
	const Bool isHDR = stbi_is_hdr(path.data());

	int width;
	int height;
	int components;

	const int requiredComonents = 4;

	stbi_uc* imageData = stbi_load(path.data(), OUT &width, OUT &height, OUT &components, requiredComonents);

	if (!imageData)
	{
		return false;
	}

	SPT_CHECK(width > 0);
	SPT_CHECK(height > 0);

	const Uint64 dataSize = static_cast<Uint64>(width) * static_cast<Uint64>(height) * requiredComonents * (isHDR ? sizeof(Real32) : 1u);

	STBTextureDataView dataView;
	dataView.format         = isHDR ? rhi::EFragmentFormat::RGBA32_S_Float : rhi::EFragmentFormat::RGBA8_UN_Float;
	dataView.resolution     = math::Vector3u(static_cast<Uint32>(width), static_cast<Uint32>(height), 1u);
	dataView.mipLevelsNum   = 1u;
	dataView.arrayLayersNum = 1u;
	dataView.imageData      = imageData;
	dataView.imageDataSize  = dataSize;

	callback(dataView);

	stbi_image_free(imageData);

	return true;
}


Bool SaveTextureImpl(const lib::SharedRef<rdr::Texture>& texture, const lib::String& path)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RHITexture& rhiTexture        = texture->GetRHI();
	const rhi::TextureDefinition& textureDef = rhiTexture.GetDefinition();

	const math::Vector3u resolution3D = textureDef.resolution.AsVector();

	SPT_CHECK(resolution3D.z() == 1u); // 2D texture

	const math::Vector2i resolution = resolution3D.head<2>().cast<int>();

	SPT_CHECK(textureDef.mipLevels == 1u);
	SPT_CHECK(textureDef.arrayLayers == 1u);

	SPT_CHECK(textureDef.format == rhi::EFragmentFormat::RGBA8_UN_Float || textureDef.format == rhi::EFragmentFormat::RGBA32_S_Float);

	const Bool isHDR = textureDef.format == rhi::EFragmentFormat::RGBA32_S_Float;
	const int componentsNum = 4;

	const rhi::RHIMappedTexture mappedTexture(rhiTexture);
	const rhi::RHIMappedSurface mappedSurface = mappedTexture.GetSurface(0u, 0u);

	const lib::Span<const Byte> textureData = mappedSurface.GetMipData();

	int result = 0;

	std::ofstream stream = lib::File::OpenOutputStream(path, lib::Flags(lib::EFileOpenFlags::ForceCreate, lib::EFileOpenFlags::DiscardContent, lib::EFileOpenFlags::Binary));
	SPT_CHECK(stream.is_open());

	const auto writer = [](void* context, void* data, int size)
	{
		std::ostream& stream = *reinterpret_cast<std::ostream*>(context);
		stream.write(reinterpret_cast<const char*>(data), size);
	};

	if (isHDR)
	{
		result = stbi_write_hdr_to_func(writer, &stream, resolution.x(), resolution.y(), componentsNum, reinterpret_cast<const Real32*>(textureData.data()));
	}
	else
	{
		result = stbi_write_png_to_func(writer, &stream, resolution.x(), resolution.y(), componentsNum, textureData.data(), mappedSurface.GetRowStride());
	}

	stream.close();

	return result != 0;
}

} // png_jpg

namespace dds
{

using DDSHeader = ::dds::Header;


struct DDSTextureDataView : public TextureDataView
{
	// Begin TextureDataView overrides
	virtual lib::Span<const Byte> GetSurfaceData(Uint32 mipLevel, Uint32 arrayLayer) const override
	{
		SPT_CHECK(!!header);
		SPT_CHECK(mipLevel < header->mip_levels());
		SPT_CHECK(arrayLayer < header->array_size());

		const Uint64 dataOffset = header->mip_offset(mipLevel, arrayLayer);
		const Uint64 dataSize   = header->mip_size(mipLevel);

		SPT_CHECK(dataOffset + dataSize <= data.size());
		SPT_CHECK(dataSize > 0u);

		return { data.data() + dataOffset, dataSize };
	}
	// End TextureDataView overrides

	lib::Span<const Byte> data;

	const DDSHeader* header = nullptr;
};


template<typename TCallback>
Bool LoadTextureImpl(TCallback&& callback, lib::StringView path)
{
	std::ifstream stream = lib::File::OpenInputStream(lib::String(path), lib::EFileOpenFlags::Binary);
	if (stream.fail())
	{
		return false;
	}

	stream.seekg(0, std::ios::end);
	const SizeType size = stream.tellg();

	stream.seekg(0, std::ios::beg);

	lib::DynamicArray<Byte> fileData(size);

	stream.read(reinterpret_cast<char*>(fileData.data()), size);
	stream.close();

	const DDSHeader header = ::dds::read_header(fileData.data(), size);

	if (!header.is_valid())
	{
		return false;
	}

	DDSTextureDataView dataView;
	dataView.format         = rhi::DXGI2RHIFormat(static_cast<DXGI_FORMAT>(header.format()));
	dataView.resolution     = math::Vector3u(header.width(), header.height(), header.depth());
	dataView.mipLevelsNum   = header.mip_levels();
	dataView.arrayLayersNum = header.array_size();
	dataView.data           = { fileData };
	dataView.header         = &header;

	callback(dataView);

	return true;
}

Bool SaveTextureImpl(math::Vector3u resolution, rhi::EFragmentFormat format, lib::Span<const Byte> dataLinear, const lib::String& path)
{
	SPT_PROFILER_FUNCTION();

	const Bool isCubemap = false;

	const Uint32 mipLevelsNum   = 1u;
	const Uint32 arrayLayersNum = 1u;

	dds::DDSHeader header;
	::dds::write_header(&header, static_cast<::dds::DXGI_FORMAT>(rhi::RHI2DXGIFormat(format)),
						resolution.x(),
						resolution.y(),
						mipLevelsNum,
						arrayLayersNum,
						isCubemap,
						resolution.z() > 1u ? resolution.z() : 0u);

	lib::DynamicArray<Byte> outData(sizeof(dds::DDSHeader) + header.data_size());
	std::memcpy(outData.data(), &header, sizeof(header));

	const Uint64 pixelSize = rhi::GetFragmentSize(format);

	const Uint64 srcRowSize   = resolution.x() * pixelSize;
	const Uint64 srcSliceSize = srcRowSize * resolution.y();

	const Uint64 dstDataOffset = header.mip_offset(0u, 0u);

	const Uint64 dstSlicePitch = header.slice_pitch(0u);
	const Uint64 dstRowPitch   = header.row_pitch(0u);

	SPT_CHECK(srcRowSize == dstRowPitch);

	for (Uint32 z = 0u; z < resolution.z(); ++z)
	{
		const Uint64 dstSliceOffset = dstDataOffset + dstSlicePitch * z;
		const Uint64 srcSliceOffset = srcSliceSize * z;
		for (Uint32 y = 0u; y < resolution.y(); ++y)
		{
			const Uint64 dstRowOffset = dstSliceOffset + dstRowPitch * y;
			const Uint64 srcRowOffset = srcSliceOffset + srcRowSize * y;

			std::memcpy(&outData[dstRowOffset], dataLinear.data() + srcRowOffset, srcRowSize);
		}
	}
	
	std::ofstream stream = lib::File::OpenOutputStream(path, lib::Flags(lib::EFileOpenFlags::ForceCreate, lib::EFileOpenFlags::DiscardContent, lib::EFileOpenFlags::Binary));

	if (stream.is_open())
	{
		stream.write(reinterpret_cast<const char*>(outData.data()), outData.size());
		stream.close();
		return true;
	}

	return false;
}

Bool SaveTextureImpl(const lib::SharedRef<rdr::Texture>& texture, const lib::String& path)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RHITexture& rhiTexture        = texture->GetRHI();
	const rhi::TextureDefinition& textureDef = rhiTexture.GetDefinition();

	const math::Vector3u resolution = textureDef.resolution.AsVector();

	const Bool isCubemap = false;

	dds::DDSHeader header;
	::dds::write_header(&header, static_cast<::dds::DXGI_FORMAT>(rhi::RHI2DXGIFormat(textureDef.format)),
						resolution.x(),
						resolution.y(),
						textureDef.mipLevels,
						textureDef.arrayLayers,
						isCubemap,
						rhiTexture.GetType() == rhi::ETextureType::Texture3D ? resolution.z() : 0u);

	lib::DynamicArray<Byte> outData(sizeof(dds::DDSHeader) + header.data_size());
	std::memcpy(outData.data(), &header, sizeof(header));

	for (Uint32 mipLevelIdx = 0u; mipLevelIdx < textureDef.mipLevels; ++mipLevelIdx)
	{
		const math::Vector3u mipResolution = rhiTexture.GetMipResolution(mipLevelIdx);

		for (Uint32 arrayLayerIdx = 0u; arrayLayerIdx < textureDef.arrayLayers; ++arrayLayerIdx)
		{
			const rhi::RHIMappedTexture mappedTexture(rhiTexture);
			const rhi::RHIMappedSurface mappedSurface = mappedTexture.GetSurface(mipLevelIdx, arrayLayerIdx);

			for (Uint32 depth = 0u; depth < mipResolution.z(); ++depth)
			{
				for (Uint32 row = 0u; row < mipResolution.y(); ++row)
				{
					const lib::Span<const Byte> rowData = mappedSurface.GetRowData(row, depth);
					const Uint64 dataOffset = header.mip_offset(mipLevelIdx, arrayLayerIdx) + header.row_pitch(mipLevelIdx) * row;

					std::memcpy(outData.data() + dataOffset, rowData.data(), rowData.size());
				}
			}
		}
	}
	
	std::ofstream stream = lib::File::OpenOutputStream(path, lib::Flags(lib::EFileOpenFlags::ForceCreate, lib::EFileOpenFlags::DiscardContent, lib::EFileOpenFlags::Binary));

	if (stream.is_open())
	{
		stream.write(reinterpret_cast<const char*>(outData.data()), outData.size());
		stream.close();
		return true;
	}

	return false;
}

} // dds

//////////////////////////////////////////////////////////////////////////////////////////////////
// TextureLoader =================================================================================

lib::SharedPtr<rdr::Texture> TextureLoader::LoadTexture(lib::StringView path, rhi::ETextureUsage usage /*= TextureLoadParams::defaultUsage*/, rhi::EMemoryUsage memoryUsage /*= rhi::EMemoryUsage::GPUOnly*/)
{
	SPT_PROFILER_FUNCTION();

	TextureLoadParams params;
	params.memoryUsage = memoryUsage;
	params.usage       = usage;

	return LoadTexture(path, params);
}

lib::SharedPtr<rdr::Texture> TextureLoader::LoadTexture(lib::StringView path, const TextureLoadParams& params)
{
	SPT_PROFILER_FUNCTION();

	lib::SharedPtr<rdr::Texture> loadedTexture;

	const auto callback = [&loadedTexture, &path, &params](const TextureDataView& dataView)
	{
		rhi::TextureDefinition textureDefinition;
		textureDefinition.resolution  = dataView.resolution;
		textureDefinition.usage       = params.usage;
		textureDefinition.format      = dataView.format;
		textureDefinition.mipLevels   = dataView.mipLevelsNum;
		textureDefinition.arrayLayers = dataView.arrayLayersNum;

		if (params.forceTiling)
		{
			textureDefinition.tiling = *params.forceTiling;
		}

		loadedTexture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME(path), textureDefinition, params.memoryUsage);

		const rhi::ETextureAspect aspect = rhi::GetFullAspectForFormat(textureDefinition.format);

		for (Uint32 mipLevelIdx = 0u; mipLevelIdx < dataView.mipLevelsNum; ++mipLevelIdx)
		{
			for (Uint32 arrayLayerIdx = 0u; arrayLayerIdx < dataView.arrayLayersNum; ++arrayLayerIdx)
			{
				const math::Vector3u mipResolution = loadedTexture->GetRHI().GetMipResolution(mipLevelIdx);

				const lib::Span<const Byte> data = dataView.GetSurfaceData(mipLevelIdx, arrayLayerIdx);

				UploadsManager::Get().EnqueUploadToTexture(data.data(), data.size(), lib::Ref(loadedTexture), aspect, mipResolution,  math::Vector3u::Zero(), mipLevelIdx, arrayLayerIdx);
			}
		}
	};

	const lib::StringView extension = lib::File::GetExtension(path);

	if (extension == "png" || extension == "jpg" || extension == "jpeg")
	{
		png_jpg::LoadTextureImpl(callback, path);
	}
	else if (extension == "dds")
	{
		dds::LoadTextureImpl(callback, path);
	}
	else
	{
		SPT_LOG_ERROR(ImageLoader, "Unsupported texture format: {} (path: {})", extension, path);
	}

	return loadedTexture;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// TextureWriter =================================================================================

Bool TextureWriter::SaveTexture(math::Vector3u resolution, rhi::EFragmentFormat format, lib::Span<const Byte> dataLinear, const lib::String& path)
{
	SPT_PROFILER_FUNCTION();

	const lib::StringView extension = lib::File::GetExtension(path);

	if (extension == "png")
	{
		SPT_CHECK_NO_ENTRY_MSG("Not implemented yet");

		return false;
	}
	else if (extension == "dds")
	{
		return dds::SaveTextureImpl(resolution, format, dataLinear, path);
	}
	else
	{
		SPT_LOG_ERROR(ImageLoader, "Unsupported texture format: {} (path: {})", extension, path);

		return false;
	}
}

Bool TextureWriter::SaveTexture(lib::SharedRef<rdr::Texture> texture, const lib::String& path)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(texture->GetRHI().GetDefinition().tiling == rhi::ETextureTiling::Linear);

	const lib::StringView extension = lib::File::GetExtension(path);

	if (extension == "png")
	{
		return png_jpg::SaveTextureImpl(texture, path);
	}
	else if (extension == "dds")
	{
		return dds::SaveTextureImpl(texture, path);
	}
	else
	{
		SPT_LOG_ERROR(ImageLoader, "Unsupported texture format: {} (path: {})", extension, path);

		return false;
	}
}

js::JobWithResult<Bool> TextureWriter::SaveTexture(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle textureView, lib::String path)
{
	SPT_PROFILER_FUNCTION();

	const lib::SharedRef<rdr::Texture> textureData = graphBuilder.DownloadTexture(RG_DEBUG_NAME("Texture Download"), textureView);

	const math::Vector2u resolution   = textureView->GetResolution2D();

	return js::Launch(SPT_GENERIC_JOB_NAME,
					  [textureData, filePath = std::move(path)]
					  {
						  return TextureWriter::SaveTexture(textureData, filePath);
					  },
					  js::Prerequisites(graphBuilder.GetGPUFinishedEvent()));
}

} // spt::gfx
