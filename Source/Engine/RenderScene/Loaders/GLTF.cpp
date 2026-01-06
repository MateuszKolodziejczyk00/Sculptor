#include "GLTF.h"
#include "Paths.h"


namespace spt::rsc
{

std::optional<GLTFModel> LoadGLTFModel(const lib::String& path)
{
	SPT_PROFILER_FUNCTION();

	const lib::StringView fileExtension = engn::Paths::GetExtension(path);
	SPT_CHECK(fileExtension == "gltf" || fileExtension == "glb");

	tinygltf::Model model;
	tinygltf::TinyGLTF loader;

	loader.SetStoreOriginalJSONForExtrasAndExtensions(false);

	lib::String error;
	lib::String warning;

	const Bool isBinary = fileExtension == "glb";

	const Bool loaded = isBinary ? loader.LoadBinaryFromFile(&model, &error, &warning, path)
								 : loader.LoadASCIIFromFile(&model, &error, &warning, path);

	return loaded ? std::optional<GLTFModel>(std::move(model)) : std::nullopt;
}

} // spt::rsc
