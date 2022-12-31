#include "MeshBuilder.h"

namespace spt::rsc
{

MeshBuilder::MeshBuilder()
{ }

MeshGeometryData MeshBuilder::EmitMeshGeometry()
{
	SPT_PROFILER_FUNCTION();

	GeometryManager& geomManager = GeometryManager::Get();
	const rhi::RHISuballocation geometrySuballocation = geomManager.CreateGeometry(m_geometryData.data(), m_geometryData.size());
	SPT_CHECK(geometrySuballocation.IsValid());

	SPT_CHECK(geometrySuballocation.GetOffset() + m_geometryData.size() <= maxValue<Uint32>);
	const Uint32 baseOffset = static_cast<Uint32>(geometrySuballocation.GetOffset());

	for (PrimitiveGeometryInfo& prim : m_primitives)
	{
		prim.indicesOffset += baseOffset;
		prim.locationsOffset += baseOffset;
		prim.normalsOffset += baseOffset;
		prim.tangentsOffset += baseOffset;
		prim.uvsOffset += baseOffset;
	}

	const rhi::RHISuballocation primitivesSuballocation = geomManager.CreatePrimitives(m_primitives.data(), static_cast<Uint32>(m_primitives.size()));
	SPT_CHECK(primitivesSuballocation.IsValid());
	SPT_CHECK(primitivesSuballocation.GetOffset() % sizeof(PrimitiveGeometryInfo) == 0);

	MeshGeometryData mesh;
	mesh.firstPrimitiveIdx = static_cast<Uint32>(primitivesSuballocation.GetOffset() / sizeof(PrimitiveGeometryInfo));
	mesh.primitivesNum = static_cast<Uint32>(m_primitives.size());
	mesh.primtivesSuballocation = primitivesSuballocation;
	mesh.geometrySuballocation = geometrySuballocation;

	return mesh;
}

void MeshBuilder::BeginNewPrimitive()
{
	m_primitives.emplace_back();
}

PrimitiveGeometryInfo& MeshBuilder::GetBuiltPrimitive()
{
	SPT_CHECK(!m_primitives.empty());
	return m_primitives.back();
}

Uint64 MeshBuilder::GetCurrentDataSize() const
{
	return m_geometryData.size();
}

Byte* MeshBuilder::AppendData(Uint64 appendSize)
{
	const SizeType prevSize = m_geometryData.size();
	m_geometryData.resize(m_geometryData.size() + appendSize);
	return &m_geometryData[prevSize];
}

} // spt::rsc
