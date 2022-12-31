#include "glTFSceneLoader.h"
#include "Paths.h"
#include "MeshBuilder.h"

#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable: 4996)
#include "tiny_gltf.h"
#pragma warning(pop)

namespace spt::rsc
{

SPT_DEFINE_LOG_CATEGORY(LogGLTFLoader, true);

//////////////////////////////////////////////////////////////////////////////////////////////////
// GLTFPrimitiveBuilder ==========================================================================

class GLTFMeshBuilder : public MeshBuilder
{
public:

	GLTFMeshBuilder();

	void BuildMesh(const tinygltf::Mesh& mesh, const tinygltf::Model& model);

private:

	const tinygltf::Accessor* GetAttributeAccessor(const tinygltf::Primitive& prim, const tinygltf::Model& model, const lib::String& name);

	void SetIndices(const tinygltf::Accessor& accessor, const tinygltf::Model& model);
	void SetLocations(const tinygltf::Accessor& accessor, const tinygltf::Model& model);
	void SetNormals(const tinygltf::Accessor& accessor, const tinygltf::Model& model);
	void SetTangents(const tinygltf::Accessor& accessor, const tinygltf::Model& model);
	void SetUVs(const tinygltf::Accessor& accessor, const tinygltf::Model& model);

	template<typename TDestType>
	Uint32 AppendAccessorData(const tinygltf::Accessor& accessor, const tinygltf::Model& model);
};

GLTFMeshBuilder::GLTFMeshBuilder()
{ }

void GLTFMeshBuilder::BuildMesh(const tinygltf::Mesh& mesh, const tinygltf::Model& model)
{
	SPT_PROFILER_FUNCTION();

	for (const tinygltf::Primitive& prim : mesh.primitives)
	{
		BeginNewPrimitive();

		SetIndices(model.accessors[prim.indices], model);

		const static lib::String locationsAccessorName = "POSITION";
		if (const tinygltf::Accessor* locationsAccessor = GetAttributeAccessor(prim, model, locationsAccessorName))
		{
			SetLocations(*locationsAccessor, model);
		}

		const static lib::String normalsAccessorName = "NORMAL";
		if (const tinygltf::Accessor* normalsAccessor = GetAttributeAccessor(prim, model, normalsAccessorName))
		{
			SetNormals(*normalsAccessor, model);
		}

		const static lib::String tangentsAccessorName = "TANGENT";
		if (const tinygltf::Accessor* tangentsAccessor = GetAttributeAccessor(prim, model, tangentsAccessorName))
		{
			SetTangents(*tangentsAccessor, model);
		}

		const static lib::String uvsAccessorName = "TEXCOOORD_0";
		if (const tinygltf::Accessor* uvsAccessor = GetAttributeAccessor(prim, model, uvsAccessorName))
		{
			SetUVs(*uvsAccessor, model);
		}
	}
}

const tinygltf::Accessor* GLTFMeshBuilder::GetAttributeAccessor(const tinygltf::Primitive& prim, const tinygltf::Model& model, const lib::String& name)
{
	const auto foundAccessorIdxIt = prim.attributes.find(name);
	const Int32 foundAccessorIdx = foundAccessorIdxIt != std::cend(prim.attributes) ? foundAccessorIdxIt->second : -1;
	return foundAccessorIdx != -1 ? &model.accessors[foundAccessorIdx] : nullptr;
}

void GLTFMeshBuilder::SetIndices(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	PrimitiveGeometryInfo& prim = GetBuiltPrimitive();
	SPT_CHECK(prim.indicesNum == idxNone<Uint32>);

	prim.indicesOffset = static_cast<Uint32>(GetCurrentDataSize());
	prim.indicesNum = AppendAccessorData<Uint32>(accessor, model);
}

void GLTFMeshBuilder::SetLocations(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	PrimitiveGeometryInfo& prim = GetBuiltPrimitive();
	SPT_CHECK_MSG(prim.indicesNum != idxNone<Uint32>, "Indices must be set before all vertex attributes");
	SPT_CHECK(prim.locationsOffset == idxNone<Uint32>);
	
	prim.locationsOffset = static_cast<Uint32>(GetCurrentDataSize());
	AppendAccessorData<Real32>(accessor, model);
}

void GLTFMeshBuilder::SetNormals(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	PrimitiveGeometryInfo& prim = GetBuiltPrimitive();
	SPT_CHECK_MSG(prim.indicesNum != idxNone<Uint32>, "Indices must be set before all vertex attributes");
	SPT_CHECK(prim.normalsOffset == idxNone<Uint32>);
	
	prim.normalsOffset = static_cast<Uint32>(GetCurrentDataSize());
	AppendAccessorData<Real32>(accessor, model);
}

void GLTFMeshBuilder::SetTangents(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	PrimitiveGeometryInfo& prim = GetBuiltPrimitive();
	SPT_CHECK_MSG(prim.indicesNum != idxNone<Uint32>, "Indices must be set before all vertex attributes");
	SPT_CHECK(prim.tangentsOffset == idxNone<Uint32>);
	
	prim.tangentsOffset = static_cast<Uint32>(GetCurrentDataSize());
	AppendAccessorData<Real32>(accessor, model);
}

void GLTFMeshBuilder::SetUVs(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	PrimitiveGeometryInfo& prim = GetBuiltPrimitive();
	SPT_CHECK_MSG(prim.indicesNum != idxNone<Uint32>, "Indices must be set before all vertex attributes");
	SPT_CHECK(prim.uvsOffset == idxNone<Uint32>);
	
	prim.uvsOffset = static_cast<Uint32>(GetCurrentDataSize());
	AppendAccessorData<Real32>(accessor, model);
}

template<typename TDestType>
Uint32 GLTFMeshBuilder::AppendAccessorData(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	SPT_PROFILER_FUNCTION();

	const Int32 bufferViewIdx = accessor.bufferView;
	SPT_CHECK(bufferViewIdx != -1);

	const tinygltf::BufferView& bufferView = model.bufferViews[bufferViewIdx];
	const Int32 bufferIdx = bufferView.buffer;
	SPT_CHECK(bufferIdx != -1);

	const tinygltf::Buffer& buffer = model.buffers[bufferIdx];

	const SizeType bufferByteOffset = accessor.byteOffset + bufferView.byteOffset;
	const SizeType stride = bufferView.byteStride;

	const SizeType elementsNum = accessor.count;
	const SizeType componentsNum = tinygltf::GetNumComponentsInType(accessor.type);
	const Int32 componentType = accessor.componentType;
	
	const unsigned char* data = buffer.data.data() + bufferByteOffset;

	switch (componentType)
	{
	case TINYGLTF_COMPONENT_TYPE_BYTE:
		AppendData<TDestType, char>(buffer.data.data(), componentsNum, stride, elementsNum);
		break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
		AppendData<TDestType, unsigned char>(data, componentsNum, stride, elementsNum);
		break;
	case TINYGLTF_COMPONENT_TYPE_SHORT:
		AppendData<TDestType, short>(data, componentsNum, stride, elementsNum);
		break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		AppendData<TDestType, unsigned short>(data, componentsNum, stride, elementsNum);
		break;
	case TINYGLTF_COMPONENT_TYPE_INT:
		AppendData<TDestType, Int32>(data, componentsNum, stride, elementsNum);
		break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
		AppendData<TDestType, Uint32>(data, componentsNum, stride, elementsNum);
		break;
	case TINYGLTF_COMPONENT_TYPE_FLOAT:
		AppendData<TDestType, Real32>(data, componentsNum, stride, elementsNum);
		break;
	case TINYGLTF_COMPONENT_TYPE_DOUBLE:
		AppendData<TDestType, Real64>(data, componentsNum, stride, elementsNum);
		break;
	default:
		SPT_CHECK_NO_ENTRY();
	}

	// value must be lower beceuase maxValue is reserved for "invalid"
	SPT_CHECK(elementsNum < maxValue<Uint32>);
	return static_cast<Uint32>(elementsNum);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// glTFLoader ====================================================================================

namespace glTFLoader
{

static void LoadMesh(const tinygltf::Mesh& mesh, const tinygltf::Model& model)
{
	SPT_PROFILER_FUNCTION();

	GLTFMeshBuilder builder;
	builder.BuildMesh(mesh, model);

	builder.EmitMeshGeometry();
}

void LoadScene(RenderScene& scene, lib::StringView path)
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

	lib::String pathAsString(path);

	const Bool loaded = isBinary ? loader.LoadBinaryFromFile(&model, &error, &warning, pathAsString)
								 : loader.LoadASCIIFromFile(&model, &error, &warning, pathAsString);

	if (!warning.empty())
	{
		SPT_LOG_WARN(LogGLTFLoader, warning.data());
	}

	if (!error.empty())
	{
		SPT_LOG_ERROR(LogGLTFLoader, error.data());
	}

	if (loaded)
	{
		for (const tinygltf::Mesh& mesh : model.meshes)
		{
			LoadMesh(mesh, model);
		}
	}
}

} // glTFLoader

} // spt::rsc
