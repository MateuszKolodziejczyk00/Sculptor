#include "GLTF.h"
#include "Paths.h"

#pragma warning(push)
#pragma warning(disable: 4244)
#pragma warning(disable: 4996)
#include "stb_image.h"
#pragma warning(pop)

#include <cstring>
#include <fstream>


namespace spt::rsc
{

std::optional<GLTFModel> LoadGLTFModel(const lib::String& path)
{
	SPT_PROFILER_FUNCTION();

	const lib::String fileExtension = lib::Path(path).extension().generic_string();
	SPT_CHECK(fileExtension == ".gltf" || fileExtension == ".glb");

	cgltf_options options{};
	cgltf_data* data = nullptr;

	if (cgltf_parse_file(&options, path.c_str(), &data) != cgltf_result_success)
	{
		return std::nullopt;
	}

	if (cgltf_load_buffers(&options, data, path.c_str()) != cgltf_result_success)
	{
		cgltf_free(data);
		return std::nullopt;
	}

	return GLTFModel(data, path);
}

static lib::Span<const char*> CopyNames(lib::MemoryArena& memoryArena, lib::Span<const char* const> srcNames)
{
	lib::Span<const char*> names = memoryArena.AllocateSpanUninitialized<const char*>(srcNames.size());

	for (SizeType idx = 0u; idx < srcNames.size(); ++idx)
	{
		const char* srcName = srcNames[idx] ? srcNames[idx] : "";
		const SizeType length = std::strlen(srcName);

		lib::Span<char> name = memoryArena.AllocateSpanUninitialized<char>(length + 1u);
		std::memcpy(name.data(), srcName, length + 1u);
		names[idx] = name.data();
	}

	return names;
}

std::optional<GLTFSceneInfo> LoadGLTFSceneInfo(lib::MemoryArena& memoryArena, const lib::String& path)
{
	SPT_PROFILER_FUNCTION();

	cgltf_options options{};
	cgltf_data* data = nullptr;

	if (cgltf_parse_file(&options, path.c_str(), &data) != cgltf_result_success)
	{
		return std::nullopt;
	}

	lib::DynamicArray<const char*> meshNames;
	meshNames.reserve(data->meshes_count);
	for (const cgltf_mesh& mesh : lib::Span<const cgltf_mesh>(data->meshes, data->meshes_count))
	{
		meshNames.emplace_back(mesh.name);
	}

	lib::DynamicArray<const char*> materialNames;
	materialNames.reserve(data->materials_count);
	for (const cgltf_material& material : lib::Span<const cgltf_material>(data->materials, data->materials_count))
	{
		materialNames.emplace_back(material.name);
	}

	GLTFSceneInfo info;
	info.meshNames     = CopyNames(memoryArena, meshNames);
	info.materialNames = CopyNames(memoryArena, materialNames);

	cgltf_free(data);

	return info;
}
static std::optional<GLTFImageData> DecodeImage(const Byte* encodedData, SizeType size)
{
	const stbi_uc* src = reinterpret_cast<const stbi_uc*>(encodedData);
	const int srcSize  = static_cast<int>(size);

	int width = 0;
	int height = 0;
	int components = 0;
	if (!stbi_info_from_memory(src, srcSize, &width, &height, &components))
	{
		return std::nullopt;
	}

	const Bool is16Bit = stbi_is_16_bit_from_memory(src, srcSize) != 0;

	void* pixels = is16Bit ? static_cast<void*>(stbi_load_16_from_memory(src, srcSize, &width, &height, &components, 0))
						   : static_cast<void*>(stbi_load_from_memory(src, srcSize, &width, &height, &components, 0));
	if (!pixels)
	{
		return std::nullopt;
	}

	GLTFImageData result;
	result.width            = static_cast<Uint32>(width);
	result.height           = static_cast<Uint32>(height);
	result.componentsNum    = static_cast<Uint32>(components);
	result.bitsPerComponent = is16Bit ? 16u : 8u;

	const SizeType bytesNum = static_cast<SizeType>(width) * height * components * (is16Bit ? 2u : 1u);
	result.data.assign(static_cast<const Byte*>(pixels), static_cast<const Byte*>(pixels) + bytesNum);

	stbi_image_free(pixels);

	return result;
}

std::optional<GLTFImageData> LoadGLTFImage(const GLTFModel& model, const cgltf_image& image)
{
	SPT_PROFILER_FUNCTION();

	cgltf_options options{};

	if (image.buffer_view)
	{
		const cgltf_buffer_view& view = *image.buffer_view;
		if (!view.buffer || !view.buffer->data)
		{
			return std::nullopt;
		}

		return DecodeImage(static_cast<const Byte*>(view.buffer->data) + view.offset, view.size);
	}

	if (!image.uri)
	{
		return std::nullopt;
	}

	const lib::String uri = image.uri;

	if (uri.starts_with("data:"))
	{
		const SizeType commaPos = uri.find(',');
		if (commaPos == lib::String::npos)
		{
			return std::nullopt;
		}

		const char* base64 = uri.c_str() + commaPos + 1u;
		const SizeType base64Len = uri.size() - commaPos - 1u;

		SizeType decodedSize = (base64Len / 4u) * 3u;
		if (base64Len >= 1u && base64[base64Len - 1u] == '=')
		{
			--decodedSize;
		}
		if (base64Len >= 2u && base64[base64Len - 2u] == '=')
		{
			--decodedSize;
		}

		void* decoded = nullptr;
		if (cgltf_load_buffer_base64(&options, decodedSize, base64, &decoded) != cgltf_result_success)
		{
			return std::nullopt;
		}

		const std::optional<GLTFImageData> result = DecodeImage(static_cast<const Byte*>(decoded), decodedSize);
		free(decoded);
		return result;
	}

	lib::String decodedUri = uri;
	cgltf_decode_uri(decodedUri.data());
	decodedUri.resize(std::strlen(decodedUri.c_str()));

	const lib::Path imagePath = lib::Path(model.GetPath()).parent_path() / lib::Path(decodedUri);

	std::ifstream file(imagePath, std::ios::binary | std::ios::ate);
	if (!file.is_open())
	{
		return std::nullopt;
	}

	const SizeType fileSize = static_cast<SizeType>(file.tellg());
	file.seekg(0, std::ios::beg);

	lib::DynamicArray<Byte> fileData(fileSize);
	file.read(reinterpret_cast<char*>(fileData.data()), static_cast<std::streamsize>(fileSize));

	return DecodeImage(fileData.data(), fileSize);
}

} // spt::rsc
