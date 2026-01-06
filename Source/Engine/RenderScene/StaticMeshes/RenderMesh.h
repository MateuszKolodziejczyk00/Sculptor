#pragma once

#include "SculptorCoreTypes.h"
#include "RayTracing/RayTracingSceneTypes.h"
#include "StaticMeshGeometry.h"


namespace spt::rdr
{
class BLASBuilder;
} // spt::rdr


namespace spt::rsc
{

struct MeshletDefinition
{
	Uint16         triangleCount           = 0u;
	Uint16         vertexCount             = 0u;
	Uint32         meshletPrimitivesOffset = 0u;
	Uint32         meshletVerticesOffset   = 0u;
	Uint32         packedConeAxisAndCutoff = 0u; /* {[uint8 cone cutoff][3 x uint8 cone axis]} */
	math::Vector3f boundingSphereCenter    = {};
	Real32         boundingSphereRadius    = 0.f;
};


struct SubmeshDefinition
{
	Uint32          indicesOffset                = 0u;
	Uint32          indicesNum                   = 0u;
	Uint32          verticesNum                  = 0u;
	Uint32          locationsOffset              = 0u;

	Uint32          normalsOffset                = 0u;
	Uint32          tangentsOffset               = 0u;
	Uint32          uvsOffset                    = 0u;
	Uint32          meshletsPrimitivesDataOffset = 0u;

	Uint32          meshletsVerticesDataOffset   = 0u;
	Uint32          firstMeshletIdx              = 0u;
	Uint32          meshletsNum                  = 0u;
	Uint32          padding                      = 0u;

	math::Vector3f  boundingSphereCenter         = {};
	Real32          boundingSphereRadius         = 0.f;
};


struct MeshDefinition
{
	Uint32 meshletsOffset  = 0u;
	Uint32 geometryOffset  = 0u;

	math::Vector3f  boundingSphereCenter = {};
	Real32          boundingSphereRadius = 0.f;

	// Blob is either owned or external
	lib::DynamicArray<Byte> blob;
	lib::Span<const Byte>   blobView;

	lib::Span<const SubmeshDefinition> GetSubmeshes() const
	{
		const Uint32 submeshesNum = (meshletsOffset) / sizeof(SubmeshDefinition);
		return lib::Span<const SubmeshDefinition>(reinterpret_cast<const SubmeshDefinition*>(&blobView[0]), submeshesNum);
	}

	lib::Span<const MeshletDefinition> GetMeshlets() const
	{
		const Uint32 meshletsNum = (geometryOffset - meshletsOffset) / sizeof(MeshletDefinition);
		return lib::Span<const MeshletDefinition>(reinterpret_cast<const MeshletDefinition*>(&blobView[0] + meshletsOffset), meshletsNum);
	}

	lib::Span<const Byte> GetGeometryData() const
	{
		return lib::Span<const Byte>(&blobView[0] + geometryOffset, blobView.size() - geometryOffset);
	}
};


class RenderMesh : public lib::MTRefCounted, public RayTracingGeometryProvider
{
public:

	RenderMesh();
	~RenderMesh();

	void Initialize(const MeshDefinition& meshDef);

	void BuildBLASes(rdr::BLASBuilder& blasBuilder);

	Bool IsValid() const { return m_geometryData.geometrySuballocation.IsValid(); }

	math::Vector3f GetBoundingSphereCenter() const { return m_boundingSphereCenter; }
	Real32         GetBoundingSphereRadius() const { return m_boundingSphereRadius; }

	SubmeshGPUPtr GetSubmeshesGPUPtr() const { return m_submeshesPtr; }

	lib::Span<const SubmeshRenderingDefinition> GetSubmeshes() const { return m_submeshRenderingDefs; }

	// Begin RayTracingGeometryProvider interface
	virtual lib::Span<const RayTracingGeometryDefinition> GetRayTracingGeometries() const override { return m_rtGeometries; }
	// End RayTracingGeometryProvider interface

private:

	math::Vector3f m_boundingSphereCenter;
	Real32         m_boundingSphereRadius;

	SubmeshGPUPtr m_submeshesPtr; // points to first submesh of this mesh in global submeshes array
	lib::DynamicArray<SubmeshRenderingDefinition> m_submeshRenderingDefs;

	lib::DynamicArray<RayTracingGeometryDefinition> m_rtGeometries;

	StaticMeshGeometryData m_geometryData;
};

} // spt::rsc
