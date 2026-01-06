#include "GLTFMeshBuilder.h"


namespace spt::rsc
{

GLTFMeshBuilder::GLTFMeshBuilder(const MeshBuildParameters& parameters) : Super(parameters)
{
}

void GLTFMeshBuilder::BuildMesh(const tinygltf::Mesh& mesh, const tinygltf::Model& model)
{
	SPT_PROFILER_FUNCTION();

	for (const tinygltf::Primitive& prim : mesh.primitives)
	{
		if (prim.material == -1)
		{
			continue;
		}

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
	SubmeshDefinition& submesh = GetCurrentSubmesh();
	SPT_CHECK(submesh.indicesNum == 0);
	SPT_CHECK(submesh.indicesOffset == idxNone<Uint32>);

	submesh.indicesOffset = static_cast<Uint32>(GetCurrentDataSize());
	submesh.indicesNum = AppendAccessorData<Uint32>(accessor, model);
}

void GLTFMeshBuilder::SetLocations(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	SubmeshDefinition& submesh = GetCurrentSubmesh();
	SPT_CHECK(submesh.locationsOffset == idxNone<Uint32>);

	submesh.locationsOffset = static_cast<Uint32>(GetCurrentDataSize());
	submesh.verticesNum = AppendAccessorData<Real32>(accessor, model);
}

void GLTFMeshBuilder::SetNormals(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	SubmeshDefinition& submesh = GetCurrentSubmesh();
	SPT_CHECK(submesh.normalsOffset == idxNone<Uint32>);

	submesh.normalsOffset = static_cast<Uint32>(GetCurrentDataSize());
	AppendAccessorData<Real32>(accessor, model);
}

void GLTFMeshBuilder::SetTangents(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	SubmeshDefinition& submesh = GetCurrentSubmesh();
	SPT_CHECK(submesh.tangentsOffset == idxNone<Uint32>);

	submesh.tangentsOffset = static_cast<Uint32>(GetCurrentDataSize());
	AppendAccessorData<Real32>(accessor, model);
}

void GLTFMeshBuilder::SetUVs(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	SubmeshDefinition& submesh = GetCurrentSubmesh();
	SPT_CHECK(submesh.uvsOffset == idxNone<Uint32>);

	submesh.uvsOffset = static_cast<Uint32>(GetCurrentDataSize());
	AppendAccessorData<Real32>(accessor, model);
}

} // spt::rsc
