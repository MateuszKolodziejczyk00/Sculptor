#include "MeshBuilder.h"

#include "meshoptimizer.h"
#include "StaticMeshes/StaticMeshPrimitivesSystem.h"

namespace spt::rsc
{

MeshBuilder::MeshBuilder()
{
	m_geometryData.reserve(1024 * 1024 * 8);
}

RenderingDataEntityHandle MeshBuilder::EmitMeshGeometry()
{
	SPT_PROFILER_FUNCTION();

	for (SubmeshBuildData& submeshBD : m_submeshes)
	{
		OptimizeSubmesh(submeshBD);
		BuildMeshlets(submeshBD);
	}

	GeometryManager& geomManager = GeometryManager::Get();
	const rhi::RHISuballocation geometrySuballocation = geomManager.CreateGeometry(m_geometryData.data(), m_geometryData.size());
	SPT_CHECK(geometrySuballocation.IsValid());
	SPT_CHECK(geometrySuballocation.GetOffset() + m_geometryData.size() <= maxValue<Uint32>);

	lib::DynamicArray<SubmeshGPUData> submeshesData;
	submeshesData.reserve(m_submeshes.size());
	std::transform(std::cbegin(m_submeshes), std::cend(m_submeshes), std::back_inserter(submeshesData),
				   [](const SubmeshBuildData& submeshBD)
				   {
					   return submeshBD.submesh;
				   });

	const Uint32 indicesNum = std::accumulate(std::cbegin(submeshesData), std::cend(submeshesData), 0,
											  [](Uint32 indices, const SubmeshGPUData& submesh)
											  {
												  return indices + submesh.indicesNum;
											  });

	const Uint32 trianglesNum = indicesNum / 3;

	const StaticMeshGeometryData staticMeshGeometryData = StaticMeshUnifiedData::Get().BuildStaticMeshData(submeshesData, m_meshlets, geometrySuballocation);

	StaticMeshRenderingDefinition staticMeshRenderingDef;
	staticMeshRenderingDef.staticMeshIdx	= static_cast<Uint32>(staticMeshGeometryData.staticMeshSuballocation.GetOffset() / sizeof(StaticMeshGPUData));
	staticMeshRenderingDef.maxSubmeshesNum	= static_cast<Uint32>(submeshesData.size());
	staticMeshRenderingDef.maxMeshletsNum	= static_cast<Uint32>(m_meshlets.size());
	staticMeshRenderingDef.maxTrianglesNum	= trianglesNum;

	RenderingDataRegistry& renderingDataRegistry = RenderingData::Get();
	const RenderingDataEntity staticMeshEntity = renderingDataRegistry.create();
	const RenderingDataEntityHandle staticMeshDataHandle(renderingDataRegistry, staticMeshEntity);

	staticMeshDataHandle.emplace<StaticMeshGeometryData>(staticMeshGeometryData);
	staticMeshDataHandle.emplace<StaticMeshRenderingDefinition>(staticMeshRenderingDef);

	return staticMeshDataHandle;
}

void MeshBuilder::BeginNewSubmesh()
{
	m_submeshes.emplace_back();
}

SubmeshBuildData& MeshBuilder::GetBuiltSubmesh()
{
	SPT_CHECK(!m_submeshes.empty());
	return m_submeshes.back();
}

Uint64 MeshBuilder::GetCurrentDataSize() const
{
	return m_geometryData.size();
}

void MeshBuilder::OptimizeSubmesh(SubmeshBuildData& submeshBuildData)
{
	SPT_PROFILER_FUNCTION();

	const auto getData = [this]<typename TDataType>(std::type_identity<TDataType*>, Uint32 offset)
	{
		return reinterpret_cast<TDataType*>(&m_geometryData[offset]);
	};

	Uint32* indices = getData(std::type_identity<Uint32*>{}, submeshBuildData.submesh.indicesOffset);

	const SizeType vertexCount = static_cast<SizeType>(submeshBuildData.vertexCount);
	const SizeType indexCount = static_cast<SizeType>(submeshBuildData.submesh.indicesNum);

	meshopt_optimizeVertexCache<Uint32>(indices, indices, indexCount, vertexCount);

	lib::DynamicArray<unsigned int> remapArray(indexCount);
	meshopt_optimizeVertexFetchRemap(remapArray.data(), indices, indexCount, vertexCount);
	meshopt_remapIndexBuffer(indices, indices, indexCount, remapArray.data());

	math::Vector3f* locations = getData(std::type_identity<math::Vector3f*>{}, submeshBuildData.submesh.locationsOffset);
	meshopt_remapVertexBuffer(locations, locations, vertexCount, sizeof(math::Vector3f), remapArray.data());

	math::Vector2f* uvs = getData(std::type_identity<math::Vector2f*>{}, submeshBuildData.submesh.uvsOffset);
	meshopt_remapVertexBuffer(uvs, uvs, vertexCount, sizeof(math::Vector2f), remapArray.data());

	math::Vector3f* normals = getData(std::type_identity<math::Vector3f*>{}, submeshBuildData.submesh.normalsOffset);
	meshopt_remapVertexBuffer(normals, normals, vertexCount, sizeof(math::Vector3f), remapArray.data());

	math::Vector4f* tangents = getData(std::type_identity<math::Vector4f*>{}, submeshBuildData.submesh.tangentsOffset);
	meshopt_remapVertexBuffer(tangents, tangents, vertexCount, sizeof(math::Vector4f), remapArray.data());
}

void MeshBuilder::BuildMeshlets(SubmeshBuildData& submeshBuildData)
{
	SPT_PROFILER_FUNCTION();

	const SizeType meshletMaxTriangles = 64;
	const SizeType meshletMaxVertices = 64;

	const SizeType vertexCount = static_cast<SizeType>(submeshBuildData.vertexCount);
	const SizeType indexCount = static_cast<SizeType>(submeshBuildData.submesh.indicesNum);

	const SizeType meshletsMaxNum = meshopt_buildMeshletsBound(indexCount, meshletMaxVertices, meshletMaxTriangles);

	lib::DynamicArray<Uint32> meshletVertices(meshletsMaxNum * meshletMaxVertices);
	lib::DynamicArray<Uint8> meshletPrimitives(meshletsMaxNum * meshletMaxTriangles * 3);

	lib::DynamicArray<meshopt_Meshlet> moMeshlets(meshletsMaxNum);

	const Uint32* indices = reinterpret_cast<const Uint32*>(&m_geometryData[submeshBuildData.submesh.indicesOffset]);
	const math::Vector3f* locations = reinterpret_cast<const math::Vector3f*>(&m_geometryData[submeshBuildData.submesh.locationsOffset]);

	const Real32 coneWeigth = 0.5f;

	const SizeType finalMeshletsNum = meshopt_buildMeshlets<Uint32>(moMeshlets.data(),
																	meshletVertices.data(),
																	meshletPrimitives.data(),
																	indices,
																	indexCount,
																	reinterpret_cast<const Real32*>(locations),
																	vertexCount,
																	sizeof(math::Vector3f),
																	meshletMaxVertices,
																	meshletMaxTriangles,
																	coneWeigth);

	const SizeType meshletVertsOffset = AppendData<Uint32, Uint32>(reinterpret_cast<const unsigned char*>(meshletVertices.data()), 1, sizeof(Uint32), meshletVertices.size(), {});
	submeshBuildData.submesh.meshletsVerticesDataOffset = static_cast<Uint32>(meshletVertsOffset);

	const SizeType meshletPrimsOffset = AppendData<Uint8, Uint8>(reinterpret_cast<const unsigned char*>(meshletPrimitives.data()), 1, sizeof(Uint8), meshletPrimitives.size(), {});
	submeshBuildData.submesh.meshletsPrimitivesDataOffset = static_cast<Uint32>(meshletPrimsOffset);

	const SizeType submeshMeshletsBegin = m_meshlets.size();
	submeshBuildData.submesh.meshletsBeginIdx = static_cast<Uint32>(submeshMeshletsBegin);
	submeshBuildData.submesh.meshletsNum = static_cast<Uint32>(finalMeshletsNum);
		
	m_meshlets.resize(m_meshlets.size() + finalMeshletsNum);
	for (SizeType meshletIdx = 0; meshletIdx < finalMeshletsNum; ++meshletIdx)
	{
		const SizeType meshletBuildDataIdx = submeshMeshletsBegin + meshletIdx;
		m_meshlets[meshletBuildDataIdx].triangleCount = moMeshlets[meshletIdx].triangle_count;
		m_meshlets[meshletBuildDataIdx].meshletPrimitivesOffset = moMeshlets[meshletIdx].triangle_offset;
		m_meshlets[meshletBuildDataIdx].meshletVerticesOffset = moMeshlets[meshletIdx].vertex_offset;
	}
}

Byte* MeshBuilder::AppendData(Uint64 appendSize)
{
	const SizeType prevSize = m_geometryData.size();
	const SizeType newSize = m_geometryData.size() + appendSize;
	m_geometryData.resize(newSize);
	return &m_geometryData[prevSize];
}

} // spt::rsc
