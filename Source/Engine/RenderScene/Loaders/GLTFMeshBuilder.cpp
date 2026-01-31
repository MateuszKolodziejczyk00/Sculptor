#include "GLTFMeshBuilder.h"
#include "MathUtils.h"


namespace spt::rsc
{

namespace priv
{

template<typename TDestType, typename TSourceType>
void CopyData(lib::Span<TDestType> dst, lib::Span<TSourceType> src)
{
	SPT_CHECK(dst.size() == src.size());

	if constexpr (std::is_same_v<TDestType, TSourceType>)
	{
		memcpy(dst.data(), src.data(), src.size() * sizeof(TDestType));
	}
	else
	{
		for (SizeType idx = 0; idx < src.size(); ++idx)
		{
			dst[idx] = static_cast<TDestType>(src[idx]);
		}
	}
}

template<typename TDestType>
static lib::Span<TDestType> CopyAccessorData(lib::MemoryArena& memoryArena, const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	SPT_PROFILER_FUNCTION();

	const Int32 bufferViewIdx = accessor.bufferView;
	SPT_CHECK(bufferViewIdx != -1);

	const tinygltf::BufferView& bufferView = model.bufferViews[bufferViewIdx];
	const Int32 bufferIdx = bufferView.buffer;
	SPT_CHECK(bufferIdx != -1);

	const tinygltf::Buffer& buffer = model.buffers[bufferIdx];

	const unsigned char* srcData = buffer.data.data() + accessor.byteOffset + bufferView.byteOffset;

	const SizeType elementsNum = accessor.count;
	const SizeType componentsNum = tinygltf::GetNumComponentsInType(accessor.type);
	const Int32 componentType = accessor.componentType;

	const SizeType valuesNum = elementsNum * componentsNum;

	lib::Span<TDestType> outData = memoryArena.AllocateSpanUninitialized<TDestType>(valuesNum);

	const auto copyImpl = [outData, valuesNum]<typename TSourceType>(const TSourceType* srcPtr)
	{
		CopyData(outData, lib::Span<const TSourceType>(reinterpret_cast<const TSourceType*>(srcPtr), valuesNum));
	};

	switch (componentType)
	{
	case TINYGLTF_COMPONENT_TYPE_BYTE:
		copyImpl(reinterpret_cast<const char*>(srcData));
		break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
		copyImpl(reinterpret_cast<const unsigned char*>(srcData));
		break;
	case TINYGLTF_COMPONENT_TYPE_SHORT:
		copyImpl(reinterpret_cast<const short*>(srcData));
		break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		copyImpl(reinterpret_cast<const unsigned short*>(srcData));
		break;
	case TINYGLTF_COMPONENT_TYPE_INT:
		copyImpl(reinterpret_cast<const Int32*>(srcData));
		break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
		copyImpl(reinterpret_cast<const Uint32*>(srcData));
		break;
	case TINYGLTF_COMPONENT_TYPE_FLOAT:
		copyImpl(reinterpret_cast<const Real32*>(srcData));
		break;
	case TINYGLTF_COMPONENT_TYPE_DOUBLE:
		copyImpl(reinterpret_cast<const Real64*>(srcData));
		break;
	default:
		SPT_CHECK_NO_ENTRY();
	}

	// value must be lower because maxValue is reserved for "invalid"
	SPT_CHECK(elementsNum < maxValue<Uint32>);
	return outData;
}

} // priv

GLTFMeshBuilder::GLTFMeshBuilder(const MeshBuildParameters& parameters) : Super(parameters)
{
}

void GLTFMeshBuilder::BuildMesh(lib::MemoryArena& memoryArena, const tinygltf::Mesh& mesh, const tinygltf::Model& model)
{
	SPT_PROFILER_FUNCTION();


	for (const tinygltf::Primitive& prim : mesh.primitives)
	{
		if (prim.material == -1)
		{
			continue;
		}

		SubmeshDefinition& submesh = BeginNewSubmesh();

		{
			lib::MemoryArenaScope memScope(memoryArena);

			const lib::Span<Uint32> indices = priv::CopyAccessorData<Uint32>(memoryArena, model.accessors[prim.indices], model);

			submesh.indicesNum = static_cast<Uint32>(indices.size());
			submesh.indicesOffset = AppendData(indices);
		}

		const static lib::String locationsAccessorName = "POSITION";
		if (const tinygltf::Accessor* locationsAccessor = GetAttributeAccessor(prim, model, locationsAccessorName))
		{
			lib::MemoryArenaScope memScope(memoryArena);

			const lib::Span<Real32> locationsVals = priv::CopyAccessorData<Real32>(memoryArena, *locationsAccessor, model);
			const lib::Span<math::Vector3f> locations = lib::Span<math::Vector3f>(reinterpret_cast<math::Vector3f*>(locationsVals.data()), locationsVals.size() / 3);
			submesh.verticesNum     = static_cast<Uint32>(locations.size());
			submesh.locationsOffset = AppendData(locations);
		}

		const static lib::String normalsAccessorName = "NORMAL";
		if (const tinygltf::Accessor* normalsAccessor = GetAttributeAccessor(prim, model, normalsAccessorName))
		{
			lib::MemoryArenaScope memScope(memoryArena);

			const lib::Span<Real32> normalVals = priv::CopyAccessorData<Real32>(memoryArena, *normalsAccessor, model);
			const lib::Span<math::Vector3f> normals = lib::Span<math::Vector3f>(reinterpret_cast<math::Vector3f*>(normalVals.data()), normalVals.size() / 3);

			lib::Span<Uint32> encodedNormals = memoryArena.AllocateSpanUninitialized<Uint32>(normals.size());

			mesh_encoding::EncodeMeshNormals(encodedNormals, normals);

			submesh.normalsOffset = AppendData(encodedNormals);
		}

		const static lib::String tangentsAccessorName = "TANGENT";
		if (const tinygltf::Accessor* tangentsAccessor = GetAttributeAccessor(prim, model, tangentsAccessorName))
		{
			lib::MemoryArenaScope memScope(memoryArena);

			const lib::Span<Real32> tangentVals = priv::CopyAccessorData<Real32>(memoryArena, *tangentsAccessor, model);
			const lib::Span<math::Vector4f> tangents = lib::Span<math::Vector4f>(reinterpret_cast<math::Vector4f*>(tangentVals.data()), tangentVals.size() / 4);

			lib::Span<Uint32> encodedTangents = memoryArena.AllocateSpanUninitialized<Uint32>(tangents.size());

			mesh_encoding::EncodeMeshTangents(encodedTangents, tangents);

			submesh.tangentsOffset = AppendData(encodedTangents);
		}

		const static lib::String uvsAccessorName = "TEXCOORD_0";
		if (const tinygltf::Accessor* uvsAccessor = GetAttributeAccessor(prim, model, uvsAccessorName))
		{
			lib::MemoryArenaScope memScope(memoryArena);

			const lib::Span<Real32> uvsVals = priv::CopyAccessorData<Real32>(memoryArena, *uvsAccessor, model);
			const lib::Span<math::Vector2f> uvs = lib::Span<math::Vector2f>(reinterpret_cast<math::Vector2f*>(uvsVals.data()), uvsVals.size() / 2);

			const lib::Span<Uint32> encodedUVs = memoryArena.AllocateSpanUninitialized<Uint32>(uvs.size());

			math::Vector2f minUVs;
			math::Vector2f maxRange;
			mesh_encoding::EncodeMeshUVs(encodedUVs, uvs, minUVs, maxRange);

			submesh.uvsMin    = minUVs;
			submesh.uvsRange  = maxRange;
			submesh.uvsOffset = AppendData(encodedUVs);
		}
	}
}

const tinygltf::Accessor* GLTFMeshBuilder::GetAttributeAccessor(const tinygltf::Primitive& prim, const tinygltf::Model& model, const lib::String& name)
{
	const auto foundAccessorIdxIt = prim.attributes.find(name);
	const Int32 foundAccessorIdx = foundAccessorIdxIt != std::cend(prim.attributes) ? foundAccessorIdxIt->second : -1;
	return foundAccessorIdx != -1 ? &model.accessors[foundAccessorIdx] : nullptr;
}

} // spt::rsc
