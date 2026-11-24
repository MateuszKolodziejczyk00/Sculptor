#pragma once

#include "MeshBuilder.h"

#include "CGLTFWrapper.h"


namespace spt::rsc
{

class GLTFMeshBuilder : public MeshBuilder
{
protected:

	using Super = MeshBuilder;

public:

	explicit GLTFMeshBuilder(const MeshBuildParameters& parameters);

	void BuildMesh(lib::MemoryArena& memoryArena, const cgltf_mesh& mesh);

private:

	const cgltf_accessor* GetAttributeAccessor(const cgltf_primitive& prim, cgltf_attribute_type type, cgltf_int index);
};

} // spt::rsc
