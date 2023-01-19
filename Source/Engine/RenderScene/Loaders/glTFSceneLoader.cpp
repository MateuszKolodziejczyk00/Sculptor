#include "glTFSceneLoader.h"
#include "Paths.h"
#include "MeshBuilder.h"
#include "RenderScene.h"
#include "StaticMeshes/StaticMeshPrimitivesSystem.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable: 4996)
#include "tiny_gltf.h"
#include "RenderingDataRegistry.h"
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
	Uint32 AppendAccessorData(const tinygltf::Accessor& accessor, const tinygltf::Model& model, const lib::DynamicArray<SizeType>& remapping = lib::DynamicArray<SizeType>{});
};

GLTFMeshBuilder::GLTFMeshBuilder()
{ }

void GLTFMeshBuilder::BuildMesh(const tinygltf::Mesh& mesh, const tinygltf::Model& model)
{
	SPT_PROFILER_FUNCTION();

	for (const tinygltf::Primitive& prim : mesh.primitives)
	{
		BeginNewSubmesh();

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

		const static lib::String uvsAccessorName = "TEXCOORD_0";
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
	SubmeshBuildData& submeshBD = GetBuiltSubmesh();
	SPT_CHECK(submeshBD.submesh.indicesNum == 0);

	submeshBD.submesh.indicesOffset = static_cast<Uint32>(GetCurrentDataSize());
	submeshBD.submesh.indicesNum = AppendAccessorData<Uint32>(accessor, model);
}

void GLTFMeshBuilder::SetLocations(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	SubmeshBuildData& submeshBD = GetBuiltSubmesh();
	SPT_CHECK(submeshBD.submesh.locationsOffset == 0);
	
	submeshBD.submesh.locationsOffset = static_cast<Uint32>(GetCurrentDataSize());
	submeshBD.vertexCount = AppendAccessorData<Real32>(accessor, model, { 2, 0, 1 });
}

void GLTFMeshBuilder::SetNormals(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	SubmeshBuildData& submeshBD = GetBuiltSubmesh();
	SPT_CHECK(submeshBD.submesh.normalsOffset == 0);
	
	submeshBD.submesh.normalsOffset = static_cast<Uint32>(GetCurrentDataSize());
	AppendAccessorData<Real32>(accessor, model, { 2, 0, 1 });
}

void GLTFMeshBuilder::SetTangents(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	SubmeshBuildData& submeshBD = GetBuiltSubmesh();
	SPT_CHECK(submeshBD.submesh.tangentsOffset == 0);
	
	submeshBD.submesh.tangentsOffset = static_cast<Uint32>(GetCurrentDataSize());
	AppendAccessorData<Real32>(accessor, model, { 2, 0, 1, 3 });
}

void GLTFMeshBuilder::SetUVs(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	SubmeshBuildData& submeshBD = GetBuiltSubmesh();
	SPT_CHECK(submeshBD.submesh.uvsOffset == 0);
	
	submeshBD.submesh.uvsOffset = static_cast<Uint32>(GetCurrentDataSize());
	AppendAccessorData<Real32>(accessor, model);
}

template<typename TDestType>
Uint32 GLTFMeshBuilder::AppendAccessorData(const tinygltf::Accessor& accessor, const tinygltf::Model& model, const lib::DynamicArray<SizeType>& remapping /*= lib::DynamicArray<SizeType>{}*/)
{
	SPT_PROFILER_FUNCTION();

	const Int32 bufferViewIdx = accessor.bufferView;
	SPT_CHECK(bufferViewIdx != -1);

	const tinygltf::BufferView& bufferView = model.bufferViews[bufferViewIdx];
	const Int32 bufferIdx = bufferView.buffer;
	SPT_CHECK(bufferIdx != -1);

	const tinygltf::Buffer& buffer = model.buffers[bufferIdx];

	const SizeType bufferByteOffset = accessor.byteOffset + bufferView.byteOffset;
	const Int32 strideAsInt = accessor.ByteStride(bufferView);
	SPT_CHECK(strideAsInt != -1);
	const SizeType stride = static_cast<SizeType>(strideAsInt);

	const SizeType elementsNum = accessor.count;
	const SizeType componentsNum = tinygltf::GetNumComponentsInType(accessor.type);
	const Int32 componentType = accessor.componentType;
	
	const unsigned char* data = buffer.data.data() + bufferByteOffset;

	switch (componentType)
	{
	case TINYGLTF_COMPONENT_TYPE_BYTE:
		AppendData<TDestType, char>(data, componentsNum, stride, elementsNum, remapping);
		break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
		AppendData<TDestType, unsigned char>(data, componentsNum, stride, elementsNum, remapping);
		break;
	case TINYGLTF_COMPONENT_TYPE_SHORT:
		AppendData<TDestType, short>(data, componentsNum, stride, elementsNum, remapping);
		break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		AppendData<TDestType, unsigned short>(data, componentsNum, stride, elementsNum, remapping);
		break;
	case TINYGLTF_COMPONENT_TYPE_INT:
		AppendData<TDestType, Int32>(data, componentsNum, stride, elementsNum, remapping);
		break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
		AppendData<TDestType, Uint32>(data, componentsNum, stride, elementsNum, remapping);
		break;
	case TINYGLTF_COMPONENT_TYPE_FLOAT:
		AppendData<TDestType, Real32>(data, componentsNum, stride, elementsNum, remapping);
		break;
	case TINYGLTF_COMPONENT_TYPE_DOUBLE:
		AppendData<TDestType, Real64>(data, componentsNum, stride, elementsNum, remapping);
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

static math::Affine3f GetNodeTransform(const tinygltf::Node& node)
{
	if (!node.matrix.empty())
	{
		SPT_CHECK(node.matrix.size() == 16);
		const math::Matrix4f matrix(math::Map<const math::Matrix4d>(node.matrix.data()).cast<float>());
		return math::Affine3f(matrix);
	}

	math::Affine3f transform = math::Affine3f::Identity();
	
	if (!node.scale.empty())
	{
		SPT_CHECK(node.scale.size() == 3);
		const math::AlignedScaling3f nodeScale(math::Map<const math::Vector3d>(node.scale.data()).cast<float>());
		transform *= nodeScale;
	}

	if (!node.rotation.empty())
	{
		SPT_CHECK(node.rotation.size() == 4);
		const math::Quaternionf nodeQuaternion(math::Map<const math::Vector4d>(node.rotation.data()).cast<float>());
		transform *= nodeQuaternion.matrix();
	}
	
	if (!node.translation.empty())
	{
		SPT_CHECK(node.translation.size() == 3);
		const math::Translation3f nodeTranslation(math::Map<const math::Vector3d>(node.translation.data()).cast<float>());
		transform *= nodeTranslation;
	}

	return transform;
}

static RenderingDataEntityHandle LoadMesh(const tinygltf::Mesh& mesh, const tinygltf::Model& model)
{
	SPT_PROFILER_FUNCTION();

	GLTFMeshBuilder builder;
	builder.BuildMesh(mesh, model);

	return builder.EmitMeshGeometry();
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
		lib::DynamicArray<RenderingDataEntityHandle> loadedMeshes;
		loadedMeshes.reserve(model.meshes.size());

		for (const tinygltf::Mesh& mesh : model.meshes)
		{
			loadedMeshes.emplace_back(LoadMesh(mesh, model));
		}

		for (const tinygltf::Node& node : model.nodes)
		{
			if (node.mesh != -1)
			{
				SPT_CHECK(static_cast<SizeType>(node.mesh) < loadedMeshes.size());

				RenderInstanceData instanceData;
				instanceData.transfrom = GetNodeTransform(node);
				const RenderSceneEntityHandle meshEntity = scene.CreateEntity(instanceData);
				StaticMeshInstanceRenderData entityStaticMeshData;
				entityStaticMeshData.staticMesh = loadedMeshes[node.mesh];
				meshEntity.emplace<StaticMeshInstanceRenderData>(entityStaticMeshData);
			}
		}
	}
}

} // glTFLoader

} // spt::rsc
