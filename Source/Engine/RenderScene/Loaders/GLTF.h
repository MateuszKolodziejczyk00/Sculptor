#pragma once

#include "SculptorCoreTypes.h"
#include "Allocators/MemoryArena.h"
#include "CGLTFWrapper.h"


namespace spt::rsc
{

class GLTFModel
{
public:

	GLTFModel() = default;

	GLTFModel(cgltf_data* inData, lib::String inPath)
		: m_data(inData, [](cgltf_data* ptr) { cgltf_free(ptr); })
		, m_path(std::move(inPath))
	{ }

	const cgltf_data* operator->() const { return m_data.get(); }
	const cgltf_data& operator*() const { return *m_data; }

	const lib::String& GetPath() const { return m_path; }

private:

	lib::SharedPtr<cgltf_data> m_data;
	lib::String m_path;
};


// cgltf doesn't decode images, so this is done with stb_image (replaces tinygltf's built-in image loading)
struct GLTFImageData
{
	Uint32 width            = 0u;
	Uint32 height           = 0u;
	Uint32 componentsNum    = 0u;
	Uint32 bitsPerComponent = 8u;

	lib::DynamicArray<Byte> data;
};


std::optional<GLTFModel> LoadGLTFModel(const lib::String& path);


// Entries are indexed the same way as meshes/materials in the file. Unnamed entries have empty names.
// All strings are allocated from the arena passed to LoadGLTFSceneInfo
struct GLTFSceneInfo
{
	lib::Span<const char*> meshNames;
	lib::Span<const char*> materialNames;
};

// Parses only the glTF JSON header - buffers and images are not loaded
std::optional<GLTFSceneInfo> LoadGLTFSceneInfo(lib::MemoryArena& memoryArena, const lib::String& path);

std::optional<GLTFImageData> LoadGLTFImage(const GLTFModel& model, const cgltf_image& image);

} // spt::rsc
