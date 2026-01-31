#pragma once 

#include "SculptorCoreTypes.h"
#include "GeometryManager.h"
#include "StaticMeshes/StaticMeshGeometry.h"
#include "ECSRegistry.h"
#include "StaticMeshes/RenderMesh.h"


namespace spt::rdr
{
class BLASBuilder;
} // spt::rdr


namespace spt::rsc
{


namespace mesh_encoding
{

void EncodeMeshNormals(lib::Span<Uint32> outEncoded, lib::Span<const math::Vector3f> normals);
void EncodeMeshTangents(lib::Span<Uint32> outEncoded, lib::Span<const math::Vector4f> tangents);
void EncodeMeshUVs(lib::Span<Uint32> outEncoded, lib::Span<const math::Vector2f> uvs, math::Vector2f& outMinUVs, math::Vector2f& outUVsRange);

} // mesh_encoding


struct MeshBuildParameters
{
	MeshBuildParameters()
		: optimizeMesh(true)
		, blasBuilder(nullptr)
	{ }

	Bool optimizeMesh;

	rdr::BLASBuilder* blasBuilder;
};


class MeshBuilder abstract
{
public:

	explicit MeshBuilder(const MeshBuildParameters& parameters);

	void Build();

	MeshDefinition CreateMeshDefinition() const;

protected:

	const MeshBuildParameters& GetParameters() const;

	SubmeshDefinition& BeginNewSubmesh();
	SubmeshDefinition& GetCurrentSubmesh();

	Uint64 GetCurrentDataSize() const;

	Uint32 AppendData(lib::Span<const Byte> data);

	template<typename TType>
	Uint32 AppendData(lib::Span<TType> data)
	{
		return AppendData(lib::Span<const Byte>(reinterpret_cast<const Byte*>(data.data()), data.size() * sizeof(TType)));
	}

private:
	
	template<typename TType>
	const TType* GetGeometryData(Uint32 offset) const
	{
		return reinterpret_cast<const TType*>(&m_geometryData[offset]);
	}

	template<typename TType>
	TType* GetGeometryDataMutable(Uint32 offset)
	{
		return reinterpret_cast<TType*>(&m_geometryData[offset]);
	}
	
#if SPT_DEBUG
	void ValidateSubmesh(SubmeshDefinition& submesh) const;
#endif // SPT_DEBUG

	void ComputeBoundingSphere(SubmeshDefinition& submesh) const;

	void OptimizeSubmesh(SubmeshDefinition& submesh);
	void BuildMeshlets(SubmeshDefinition& submesh);

	void BuildMeshletsCullingData(SubmeshDefinition& submesh);

	void ComputeMeshBoundingSphere();

	Byte* AppendData(Uint64 appendSize);

	lib::DynamicArray<Byte> m_geometryData;
	
	lib::DynamicArray<SubmeshDefinition> m_submeshes;
	lib::DynamicArray<MeshletDefinition> m_meshlets;

	math::Vector3f m_boundingSphereCenter;
	Real32 m_boundingSphereRadius;

	MeshBuildParameters m_parameters;
};

} // spt::rsc
