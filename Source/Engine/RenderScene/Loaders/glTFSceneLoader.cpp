#include "glTFSceneLoader.h"
#include "Paths.h"

#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable: 4996)
#include "tiny_gltf.h"
#pragma warning(pop)

namespace spt::rsc
{

SPT_DEFINE_LOG_CATEGORY(LogglTFLoader, true);

//////////////////////////////////////////////////////////////////////////////////////////////////
// GLTFPrimitiveBuilder ==========================================================================

class GLTFPrimitiveBuilder
{
public:

	GLTFPrimitiveBuilder();

	void SetIndices(const tinygltf::Accessor& accessor, const tinygltf::Model& model);

	Uint64 GetSize() const;

	Uint32 GetIndicesNum() const;

	Uint32 GetPositionsOffset() const;
	Uint32 GetNormalsOffset() const;
	Uint32 GetTangentsOffset() const;
	Uint32 GetUvsOffset() const;

private:

	template<typename TDestType>
	Uint32 AppendData(const tinygltf::Accessor& accessor, const tinygltf::Model& model);

	template<typename TDestType, typename TSourceType>
	void AppendData(const unsigned char* data, SizeType stride, SizeType count);

	lib::DynamicArray<Byte> m_geometryData;

	Uint32 m_indicesNum;

	Uint32 m_positionsOffset;
	Uint32 m_normalsOffset;
	Uint32 m_tangentsOffset;
	Uint32 m_uvsOffset;
};

GLTFPrimitiveBuilder::GLTFPrimitiveBuilder()
	: m_indicesNum(idxNone<Uint32>)
	, m_positionsOffset(idxNone<Uint32>)
	, m_normalsOffset(idxNone<Uint32>)
	, m_tangentsOffset(idxNone<Uint32>)
	, m_uvsOffset(idxNone<Uint32>)
{ }

void GLTFPrimitiveBuilder::SetIndices(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	SPT_CHECK(m_indicesNum == idxNone<Uint32>);

	m_indicesNum = AppendData<Uint32>(accessor, model);
}

Uint64 GLTFPrimitiveBuilder::GetSize() const
{
	return m_geometryData.size();
}

Uint32 GLTFPrimitiveBuilder::GetIndicesNum() const
{
	SPT_CHECK_MSG(m_indicesNum != idxNone<Uint32>, "Indices were not loaded!");
	return m_indicesNum;
}

Uint32 GLTFPrimitiveBuilder::GetPositionsOffset() const
{
	SPT_CHECK_MSG(m_positionsOffset != idxNone<Uint32>, "Positions were not loaded!");
	return m_positionsOffset;
}

Uint32 GLTFPrimitiveBuilder::GetNormalsOffset() const
{
	SPT_CHECK_MSG(m_normalsOffset != idxNone<Uint32>, "Normals were not loaded!");
	return m_normalsOffset;
}

Uint32 GLTFPrimitiveBuilder::GetTangentsOffset() const
{
	SPT_CHECK_MSG(m_tangentsOffset != idxNone<Uint32>, "Tangents were not loaded!");
	return m_tangentsOffset;
}

Uint32 GLTFPrimitiveBuilder::GetUvsOffset() const
{
	SPT_CHECK_MSG(m_uvsOffset != idxNone<Uint32>, "UVs were not loaded!");
	return m_uvsOffset;
}

template<typename TDestType>
Uint32 GLTFPrimitiveBuilder::AppendData(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
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

	const Int32 componentType = accessor.componentType;

	const unsigned char* data = buffer.data.data() + bufferByteOffset;

	switch (componentType)
	{
	case TINYGLTF_COMPONENT_TYPE_BYTE:
		AppendData<TDestType, char>(buffer.data.data(), stride, elementsNum);
		break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
		AppendData<TDestType, unsigned char>(data, stride, elementsNum);
		break;
	case TINYGLTF_COMPONENT_TYPE_SHORT:
		AppendData<TDestType, short>(data, stride, elementsNum);
		break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		AppendData<TDestType, unsigned short>(data, stride, elementsNum);
		break;
	case TINYGLTF_COMPONENT_TYPE_INT:
		AppendData<TDestType, Int32>(data, stride, elementsNum);
		break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
		AppendData<TDestType, Uint32>(data, stride, elementsNum);
		break;
	case TINYGLTF_COMPONENT_TYPE_FLOAT:
		AppendData<TDestType, Real32>(data, stride, elementsNum);
		break;
	case TINYGLTF_COMPONENT_TYPE_DOUBLE:
		AppendData<TDestType, Real64>(data, stride, elementsNum);
		break;
	default:
		SPT_CHECK_NO_ENTRY();
	}

	// value must be lower beceuase maxValue is reserved for "invalid"
	SPT_CHECK(elementsNum < maxValue<Uint32>);
	return static_cast<Uint32>(elementsNum);
}

template<typename TDestType, typename TSourceType>
void GLTFPrimitiveBuilder::AppendData(const unsigned char* sourceData, SizeType stride, SizeType count)
{
	const SizeType prevSize = m_geometryData.size();
	const SizeType destDataSize = count * sizeof(TDestType);
	m_geometryData.resize(prevSize + destDataSize);

	Byte* destPtr = m_geometryData.data() + prevSize;

	if constexpr (std::is_same_v<TDestType, TSourceType>)
	{
		if (stride == sizeof(TDestType))
		{
			memcpy(destPtr, sourceData, count * sizeof(TDestType));
			return;
		}
	}

	for (SizeType sourceOffset = 0, destOffset = prevSize; destOffset < m_geometryData.size(); sourceOffset += stride, destOffset += sizeof(TDestType))
	{
		TDestType* dest = reinterpret_cast<TDestType*>(m_geometryData[destOffset]);
		const TSourceType* source = reinterpret_cast<const TSourceType*>(sourceData[sourceOffset]);
		*dest = static_cast<TDestType>(*source);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// glTFLoader ====================================================================================

namespace glTFLoader
{

void LoadPrimitive(const tinygltf::Primitive& prim, const tinygltf::Model& model)
{
	SPT_PROFILER_FUNCTION();

	
}

void LoadMesh(const tinygltf::Mesh& mesh, const tinygltf::Model& model)
{
	SPT_PROFILER_FUNCTION();

	for (const tinygltf::Primitive& prim : mesh.primitives)
	{
		LoadPrimitive(prim, model);
	}
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
		SPT_LOG_WARN(LogglTFLoader, warning.data());
	}

	if (!error.empty())
	{
		SPT_LOG_ERROR(LogglTFLoader, error.data());
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
