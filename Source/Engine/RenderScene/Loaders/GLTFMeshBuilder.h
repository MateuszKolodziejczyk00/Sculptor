#pragma once

#include "MeshBuilder.h"

#include "TinyGLTF.h"


namespace spt::rsc
{

class GLTFMeshBuilder : public MeshBuilder
{
protected:

	using Super = MeshBuilder;

public:

	explicit GLTFMeshBuilder(const MeshBuildParameters& parameters);

	void BuildMesh(lib::MemoryArena& memoryArena, const tinygltf::Mesh& mesh, const tinygltf::Model& model);

private:

	const tinygltf::Accessor* GetAttributeAccessor(const tinygltf::Primitive& prim, const tinygltf::Model& model, const lib::String& name);
};

} // spt::rsc
