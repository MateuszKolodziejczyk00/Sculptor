#include "RenderMesh.h"


namespace spt::rsc
{

RenderMesh::RenderMesh()
{

}

RenderMesh::~RenderMesh()
{

}

void RenderMesh::Initialize(const MeshDefinition& meshDef)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsValid());

	const lib::Span<const SubmeshDefinition> submeshes = meshDef.GetSubmeshes();
	const lib::Span<const MeshletDefinition> meshlets  = meshDef.GetMeshlets();
	const lib::Span<const Byte> geometry               = meshDef.GetGeometryData();

	GeometryManager& geomManager = GeometryManager::Get();
	const rhi::RHIVirtualAllocation geometrySuballocation = geomManager.CreateGeometry(geometry.data(), geometry.size());
	SPT_CHECK(geometrySuballocation.IsValid());
	SPT_CHECK(geometrySuballocation.GetOffset() + geometry.size() <= maxValue<Uint32>);

	lib::DynamicArray<SubmeshGPUData> submeshesData;
	submeshesData.reserve(submeshes.size());

	for (const SubmeshDefinition& submesh : submeshes)
	{
		SubmeshGPUData& gpuSubmesh = submeshesData.emplace_back();
		gpuSubmesh.indicesOffset                = submesh.indicesOffset;
		gpuSubmesh.indicesNum                   = submesh.indicesNum;
		gpuSubmesh.locationsOffset              = submesh.locationsOffset;
		gpuSubmesh.normalsOffset                = submesh.normalsOffset;
		gpuSubmesh.tangentsOffset               = submesh.tangentsOffset;
		gpuSubmesh.uvsOffset                    = submesh.uvsOffset;
		gpuSubmesh.meshletsPrimitivesDataOffset = submesh.meshletsPrimitivesDataOffset;
		gpuSubmesh.meshletsVerticesDataOffset   = submesh.meshletsVerticesDataOffset;
		gpuSubmesh.meshlets                     = MeshletsGPUSpan(submesh.firstMeshletIdx * sizeof(MeshletGPUData), submesh.meshletsNum);
		gpuSubmesh.boundingSphereCenter         = submesh.boundingSphereCenter;
		gpuSubmesh.boundingSphereRadius         = submesh.boundingSphereRadius;
	}

	lib::DynamicArray<MeshletGPUData> meshletsData;
	meshletsData.reserve(meshlets.size());
	for (const MeshletDefinition& meshlet : meshlets)
	{
		MeshletGPUData& gpuMeshlet = meshletsData.emplace_back();
		gpuMeshlet.triangleCount           = meshlet.triangleCount;
		gpuMeshlet.vertexCount             = meshlet.vertexCount;
		gpuMeshlet.meshletPrimitivesOffset = meshlet.meshletPrimitivesOffset;
		gpuMeshlet.meshletVerticesOffset   = meshlet.meshletVerticesOffset;
		gpuMeshlet.packedConeAxisAndCutoff = meshlet.packedConeAxisAndCutoff;
		gpuMeshlet.boundingSphereCenter    = meshlet.boundingSphereCenter;
		gpuMeshlet.boundingSphereRadius    = meshlet.boundingSphereRadius;
	}

	m_geometryData = StaticMeshUnifiedData::Get().BuildStaticMeshData(submeshesData, meshletsData, geometrySuballocation);

	m_boundingSphereCenter = meshDef.boundingSphereCenter;
	m_boundingSphereRadius = meshDef.boundingSphereRadius;

	m_submeshesPtr = SubmeshGPUPtr(static_cast<Uint32>(m_geometryData.submeshesSuballocation.GetOffset()));

	SPT_CHECK(!submeshes.empty());
	SPT_CHECK(m_submeshRenderingDefs.empty());

	const Uint64 geometryDataDeviceAddress = GeometryManager::Get().GetGeometryBufferDeviceAddress();

	m_submeshRenderingDefs.reserve(submeshes.size());
	for (SizeType submeshIdx = 0; submeshIdx < submeshes.size(); ++submeshIdx)
	{
		const SubmeshDefinition& submesh = submeshes[submeshIdx];
		const SubmeshGPUData& gpuSubmesh = submeshesData[submeshIdx];

		SubmeshRenderingDefinition& submeshRenderingDef = m_submeshRenderingDefs.emplace_back();
		submeshRenderingDef.trianglesNum     = submesh.indicesNum / 3;;
		submeshRenderingDef.verticesNum      = submesh.verticesNum;
		submeshRenderingDef.meshletsNum      = submesh.meshletsNum;
		submeshRenderingDef.locationsAddress = geometryDataDeviceAddress + static_cast<Uint64>(gpuSubmesh.locationsOffset);
		submeshRenderingDef.indicesAddress   = geometryDataDeviceAddress + static_cast<Uint64>(gpuSubmesh.indicesOffset);
	}

	m_rtGeometries.reserve(m_submeshRenderingDefs.size());
	for(SizeType submeshIdx = 0; submeshIdx < m_submeshRenderingDefs.size(); ++submeshIdx)
	{
		RayTracingGeometryDefinition& rtGeoDef = m_rtGeometries.emplace_back();
		rtGeoDef.blas                   = nullptr;
		rtGeoDef.indicesDataUGBOffset   = submeshesData[submeshIdx].indicesOffset;
		rtGeoDef.locationsDataUGBOffset = submeshesData[submeshIdx].locationsOffset;
		rtGeoDef.uvsDataUGBOffset       = submeshesData[submeshIdx].uvsOffset;
		rtGeoDef.normalsDataUGBOffset   = submeshesData[submeshIdx].normalsOffset;
	}

	SPT_CHECK(IsValid());
}

void RenderMesh::BuildBLASes(rdr::BLASBuilder& blasBuilder)
{
	m_rtGeometries.reserve(m_submeshRenderingDefs.size());
	SPT_CHECK(m_rtGeometries.size() == m_submeshRenderingDefs.size());

	for (Uint32 submeshIdx = 0; submeshIdx < m_submeshRenderingDefs.size(); ++submeshIdx)
	{
		RayTracingGeometryDefinition& rtGeoDef = m_rtGeometries[submeshIdx];
		const SubmeshRenderingDefinition& submeshRenderingDef = m_submeshRenderingDefs[submeshIdx];

		SPT_CHECK(rtGeoDef.blas == nullptr);

		rhi::BLASDefinition submeshBLAS;
		submeshBLAS.trianglesGeometry.vertexLocationsAddress = submeshRenderingDef.locationsAddress;
		submeshBLAS.trianglesGeometry.vertexLocationsStride  = sizeof(math::Vector3f);
		submeshBLAS.trianglesGeometry.maxVerticesNum         = submeshRenderingDef.verticesNum;
		submeshBLAS.trianglesGeometry.indicesAddress         = submeshRenderingDef.indicesAddress;
		submeshBLAS.trianglesGeometry.indicesNum             = submeshRenderingDef.trianglesNum * 3;

		rtGeoDef.blas = blasBuilder.CreateBLAS(RENDERER_RESOURCE_NAME("Temp BLAS Name"), submeshBLAS);
	}
}

} // spt::rsc