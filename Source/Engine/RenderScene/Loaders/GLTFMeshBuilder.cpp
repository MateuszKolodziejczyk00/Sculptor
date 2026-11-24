#include "GLTFMeshBuilder.h"
#include "MathUtils.h"


namespace spt::rsc
{

namespace priv
{

static lib::Span<Real32> UnpackFloats(lib::MemoryArena& memoryArena, const cgltf_accessor& accessor)
{
	SPT_PROFILER_FUNCTION();

	const SizeType valuesNum = cgltf_accessor_unpack_floats(&accessor, nullptr, 0u);

	lib::Span<Real32> outData = memoryArena.AllocateSpanUninitialized<Real32>(valuesNum);
	cgltf_accessor_unpack_floats(&accessor, outData.data(), valuesNum);

	return outData;
}

static lib::Span<Uint32> UnpackIndices(lib::MemoryArena& memoryArena, const cgltf_accessor& accessor)
{
	SPT_PROFILER_FUNCTION();

	const SizeType indicesNum = accessor.count;

	lib::Span<Uint32> outData = memoryArena.AllocateSpanUninitialized<Uint32>(indicesNum);
	cgltf_accessor_unpack_indices(&accessor, outData.data(), sizeof(Uint32), indicesNum);

	// value must be lower because maxValue is reserved for "invalid"
	SPT_CHECK(indicesNum < maxValue<Uint32>);
	return outData;
}

} // priv

GLTFMeshBuilder::GLTFMeshBuilder(const MeshBuildParameters& parameters) : Super(parameters)
{
}

void GLTFMeshBuilder::BuildMesh(lib::MemoryArena& memoryArena, const cgltf_mesh& mesh)
{
	SPT_PROFILER_FUNCTION();

	for (const cgltf_primitive& prim : lib::Span<const cgltf_primitive>(mesh.primitives, mesh.primitives_count))
	{
		if (!prim.material || !prim.indices)
		{
			continue;
		}

		SubmeshDefinition& submesh = BeginNewSubmesh();

		{
			lib::MemoryArenaScope memScope(memoryArena);

			const lib::Span<Uint32> indices = priv::UnpackIndices(memoryArena, *prim.indices);

			submesh.indicesNum = static_cast<Uint32>(indices.size());
			submesh.indicesOffset = AppendData(indices);
		}

		if (const cgltf_accessor* locationsAccessor = GetAttributeAccessor(prim, cgltf_attribute_type_position, 0))
		{
			lib::MemoryArenaScope memScope(memoryArena);

			const lib::Span<Real32> locationsVals = priv::UnpackFloats(memoryArena, *locationsAccessor);
			const lib::Span<math::Vector3f> locations = lib::Span<math::Vector3f>(reinterpret_cast<math::Vector3f*>(locationsVals.data()), locationsVals.size() / 3);
			submesh.verticesNum     = static_cast<Uint32>(locations.size());
			submesh.locationsOffset = AppendData(locations);
		}

		if (const cgltf_accessor* normalsAccessor = GetAttributeAccessor(prim, cgltf_attribute_type_normal, 0))
		{
			lib::MemoryArenaScope memScope(memoryArena);

			const lib::Span<Real32> normalVals = priv::UnpackFloats(memoryArena, *normalsAccessor);
			const lib::Span<math::Vector3f> normals = lib::Span<math::Vector3f>(reinterpret_cast<math::Vector3f*>(normalVals.data()), normalVals.size() / 3);

			lib::Span<Uint32> encodedNormals = memoryArena.AllocateSpanUninitialized<Uint32>(normals.size());

			mesh_encoding::EncodeMeshNormals(encodedNormals, normals);

			submesh.normalsOffset = AppendData(encodedNormals);
		}

		if (const cgltf_accessor* tangentsAccessor = GetAttributeAccessor(prim, cgltf_attribute_type_tangent, 0))
		{
			lib::MemoryArenaScope memScope(memoryArena);

			const lib::Span<Real32> tangentVals = priv::UnpackFloats(memoryArena, *tangentsAccessor);
			const lib::Span<math::Vector4f> tangents = lib::Span<math::Vector4f>(reinterpret_cast<math::Vector4f*>(tangentVals.data()), tangentVals.size() / 4);

			lib::Span<Uint32> encodedTangents = memoryArena.AllocateSpanUninitialized<Uint32>(tangents.size());

			mesh_encoding::EncodeMeshTangents(encodedTangents, tangents);

			submesh.tangentsOffset = AppendData(encodedTangents);
		}

		if (const cgltf_accessor* uvsAccessor = GetAttributeAccessor(prim, cgltf_attribute_type_texcoord, 0))
		{
			lib::MemoryArenaScope memScope(memoryArena);

			const lib::Span<Real32> uvsVals = priv::UnpackFloats(memoryArena, *uvsAccessor);
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

const cgltf_accessor* GLTFMeshBuilder::GetAttributeAccessor(const cgltf_primitive& prim, cgltf_attribute_type type, cgltf_int index)
{
	for (const cgltf_attribute& attribute : lib::Span<const cgltf_attribute>(prim.attributes, prim.attributes_count))
	{
		if (attribute.type == type && attribute.index == index)
		{
			return attribute.data;
		}
	}

	return nullptr;
}

} // spt::rsc
