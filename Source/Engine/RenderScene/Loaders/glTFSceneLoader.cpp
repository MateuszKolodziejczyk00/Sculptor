#include "glTFSceneLoader.h"
#include "Paths.h"
#include "GeometryManager.h"

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

	gfx::PrimitiveGeometryInfo EmitPrimitive();

	void SetIndices(const tinygltf::Accessor& accessor, const tinygltf::Model& model);
	void SetLocations(const tinygltf::Accessor& accessor, const tinygltf::Model& model);
	void SetNormals(const tinygltf::Accessor& accessor, const tinygltf::Model& model);
	void SetTangents(const tinygltf::Accessor& accessor, const tinygltf::Model& model);
	void SetUVs(const tinygltf::Accessor& accessor, const tinygltf::Model& model);

	Uint64 GetSize() const;

	Uint32 GetIndicesNum() const;

	Uint32 GetLocationsOffset() const;
	Uint32 GetNormalsOffset() const;
	Uint32 GetTangentsOffset() const;
	Uint32 GetUvsOffset() const;

private:

	template<typename TDestType>
	Uint32 AppendData(const tinygltf::Accessor& accessor, const tinygltf::Model& model);

	template<typename TDestType, typename TSourceType>
	void AppendData(const unsigned char* data, SizeType componentsNum, SizeType stride, SizeType count);

	lib::DynamicArray<Byte> m_geometryData;

	Uint32 m_indicesNum;

	Uint32 m_locationsOffset;
	Uint32 m_normalsOffset;
	Uint32 m_tangentsOffset;
	Uint32 m_uvsOffset;

	rhi::RHISuballocation m_geometrySuballocation;
};

GLTFPrimitiveBuilder::GLTFPrimitiveBuilder()
	: m_indicesNum(idxNone<Uint32>)
	, m_locationsOffset(idxNone<Uint32>)
	, m_normalsOffset(idxNone<Uint32>)
	, m_tangentsOffset(idxNone<Uint32>)
	, m_uvsOffset(idxNone<Uint32>)
{ }

gfx::PrimitiveGeometryInfo GLTFPrimitiveBuilder::EmitPrimitive()
{
	SPT_PROFILER_FUNCTION();

	m_geometrySuballocation = gfx::GeometryManager::Get().CreateGeometry(m_geometryData.data(), m_geometryData.size());
	SPT_CHECK(m_geometrySuballocation.IsValid());
	SPT_CHECK(m_geometrySuballocation.GetOffset() + m_geometryData.size() < maxValue<Uint32>);

	const Uint32 baseOffset = static_cast<Uint32>(m_geometrySuballocation.GetOffset());

	gfx::PrimitiveGeometryInfo geometry{};
	geometry.indexBufferOffset	= baseOffset;
	geometry.indicesNum			= m_indicesNum;
	geometry.locationsOffset	= baseOffset + m_locationsOffset;
	geometry.normalsOffset		= baseOffset + m_normalsOffset;
	geometry.tangentsOffset		= baseOffset + m_tangentsOffset;
	geometry.uvsOffset			= baseOffset + m_uvsOffset;

	return geometry;
}

void GLTFPrimitiveBuilder::SetIndices(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	SPT_CHECK_MSG(m_geometryData.empty(), "Indices must be set before all vertex attributes");
	SPT_CHECK(m_indicesNum == idxNone<Uint32>);

	m_indicesNum = AppendData<Uint32>(accessor, model);
}

void GLTFPrimitiveBuilder::SetLocations(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	SPT_CHECK_MSG(m_indicesNum != idxNone<Uint32>, "Indices must be set before all vertex attributes");
	SPT_CHECK(m_locationsOffset == idxNone<Uint32>);
	
	m_locationsOffset = static_cast<Uint32>(m_geometryData.size());
	AppendData<Real32>(accessor, model);
}

void GLTFPrimitiveBuilder::SetNormals(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	SPT_CHECK_MSG(m_indicesNum != idxNone<Uint32>, "Indices must be set before all vertex attributes");
	SPT_CHECK(m_normalsOffset == idxNone<Uint32>);
	
	m_normalsOffset = static_cast<Uint32>(m_geometryData.size());
	AppendData<Real32>(accessor, model);
}

void GLTFPrimitiveBuilder::SetTangents(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	SPT_CHECK_MSG(m_indicesNum != idxNone<Uint32>, "Indices must be set before all vertex attributes");
	SPT_CHECK(m_tangentsOffset == idxNone<Uint32>);
	
	m_tangentsOffset = static_cast<Uint32>(m_geometryData.size());
	AppendData<Real32>(accessor, model);
}

void GLTFPrimitiveBuilder::SetUVs(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	SPT_CHECK_MSG(m_indicesNum != idxNone<Uint32>, "Indices must be set before all vertex attributes");
	SPT_CHECK(m_uvsOffset == idxNone<Uint32>);
	
	m_uvsOffset = static_cast<Uint32>(m_geometryData.size());
	AppendData<Real32>(accessor, model);
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

Uint32 GLTFPrimitiveBuilder::GetLocationsOffset() const
{
	SPT_CHECK_MSG(m_locationsOffset != idxNone<Uint32>, "Positions were not loaded!");
	return m_locationsOffset;
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

template<typename TDestType, typename TSourceType>
void GLTFPrimitiveBuilder::AppendData(const unsigned char* sourceData, SizeType componentsNum, SizeType stride, SizeType count)
{
	const SizeType prevSize = m_geometryData.size();
	const SizeType destDataSize = count * componentsNum * sizeof(TDestType);
	m_geometryData.resize(prevSize + destDataSize);

	Byte* destPtr = m_geometryData.data() + prevSize;

	if constexpr (std::is_same_v<TDestType, TSourceType>)
	{
		if (stride == sizeof(TDestType) * componentsNum)
		{
			memcpy(destPtr, sourceData, count * componentsNum * sizeof(TDestType));
			return;
		}
	}

	for (SizeType sourceOffset = 0, destOffset = prevSize; destOffset < m_geometryData.size(); sourceOffset += stride, destOffset += sizeof(TDestType))
	{
		TDestType* dest = reinterpret_cast<TDestType*>(m_geometryData[destOffset]);
		const TSourceType* source = reinterpret_cast<const TSourceType*>(sourceData[sourceOffset]);
		for (SizeType componentIdx = 0; componentIdx < componentsNum; ++componentIdx)
		{
			dest[componentIdx] = static_cast<TDestType>(source[componentIdx]);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// glTFLoader ====================================================================================

namespace glTFLoader
{

static const tinygltf::Accessor* GetAttributeAccessor(const tinygltf::Primitive& prim, const tinygltf::Model& model, const lib::String& name)
{
	const auto foundAccessorIdxIt = prim.attributes.find(name);
	const Int32 foundAccessorIdx = foundAccessorIdxIt != std::cend(prim.attributes) ? foundAccessorIdxIt->second : -1;
	return foundAccessorIdx != -1 ? &model.accessors[foundAccessorIdx] : nullptr;
}

static void LoadPrimitive(const tinygltf::Primitive& prim, const tinygltf::Model& model)
{
	SPT_PROFILER_FUNCTION();

	GLTFPrimitiveBuilder gltfBuilder;
	gltfBuilder.SetIndices(model.accessors[prim.indices], model);

	const static lib::String locationsAccessorName = "POSITION";
	if (const tinygltf::Accessor* locationsAccessor = GetAttributeAccessor(prim, model, locationsAccessorName))
	{
		gltfBuilder.SetLocations(*locationsAccessor, model);
	}

	const static lib::String normalsAccessorName = "NORMAL";
	if (const tinygltf::Accessor* normalsAccessor = GetAttributeAccessor(prim, model, normalsAccessorName))
	{
		gltfBuilder.SetNormals(*normalsAccessor, model);
	}

	const static lib::String tangentsAccessorName = "TANGENT";
	if (const tinygltf::Accessor* tangentsAccessor = GetAttributeAccessor(prim, model, tangentsAccessorName))
	{
		gltfBuilder.SetTangents(*tangentsAccessor, model);
	}

	const static lib::String uvsAccessorName = "TEXCOOORD_0";
	if (const tinygltf::Accessor* uvsAccessor = GetAttributeAccessor(prim, model, uvsAccessorName))
	{
		gltfBuilder.SetUVs(*uvsAccessor, model);
	}
}

static void LoadMesh(const tinygltf::Mesh& mesh, const tinygltf::Model& model)
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
