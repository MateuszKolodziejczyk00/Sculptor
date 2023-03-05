#include "MeshBuilder.h"
#include "StaticMeshes/StaticMeshPrimitivesSystem.h"
#include "RHICore/RHIAccelerationStructureTypes.h"
#include "ResourcesManager.h"
#include "Types/AccelerationStructure.h"

#include "meshoptimizer.h"
#include "RayTracing/RayTracingSceneTypes.h"

namespace spt::rsc
{

SPT_DEFINE_LOG_CATEGORY(LogMeshBuilder, true);

MeshBuilder::MeshBuilder(const MeshBuildParameters& parameters)
	: m_boundingSphereCenter(math::Vector3f::Zero())
	, m_boundingSphereRadius(0.f)
	, m_parameters(parameters)
{
	m_geometryData.reserve(1024 * 1024 * 8);
}

RenderingDataEntityHandle MeshBuilder::EmitMeshGeometry()
{
	SPT_PROFILER_FUNCTION();

	for (SubmeshBuildData& submeshBD : m_submeshes)
	{
#if SPT_DEBUG
		ValidateSubmesh(submeshBD);
#endif // SPT_DEBUG

		if (GetParameters().optimizeMesh)
		{
			OptimizeSubmesh(submeshBD);
		}

		BuildMeshlets(submeshBD);

		ComputeBoundingSphere(submeshBD);
	}

	ComputeMeshBoundingSphere();

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
	staticMeshRenderingDef.geometryDataOffset	= static_cast<Uint32>(staticMeshGeometryData.geometrySuballocation.GetOffset());
	staticMeshRenderingDef.submeshesBeginIdx	= static_cast<Uint32>(staticMeshGeometryData.submeshesSuballocation.GetOffset() / sizeof(SubmeshGPUData));
	staticMeshRenderingDef.submeshesNum			= static_cast<Uint32>(submeshesData.size());
	staticMeshRenderingDef.boundingSphereCenter = m_boundingSphereCenter;
	staticMeshRenderingDef.boundingSphereRadius = m_boundingSphereRadius;
	staticMeshRenderingDef.maxMeshletsNum		= static_cast<Uint32>(m_meshlets.size());
	staticMeshRenderingDef.maxTrianglesNum		= trianglesNum;

	RenderingDataRegistry& renderingDataRegistry = RenderingData::Get();
	const RenderingDataEntity staticMeshEntity = renderingDataRegistry.create();
	const RenderingDataEntityHandle staticMeshDataHandle(renderingDataRegistry, staticMeshEntity);

	staticMeshDataHandle.emplace<StaticMeshGeometryData>(staticMeshGeometryData);
	staticMeshDataHandle.emplace<StaticMeshRenderingDefinition>(staticMeshRenderingDef);

	const Uint64 geometryDataDeviceAddress = GeometryManager::Get().GetGeometryBufferDeviceAddress();

	if (GetParameters().blasBuilder)
	{
		BLASesComponent blasesComp;
		blasesComp.blases.reserve(m_submeshes.size());

		// Create BLAS for each submesh
		lib::DynamicArray<lib::SharedRef<rdr::BottomLevelAS>> submeshesBLASes;
		submeshesBLASes.reserve(m_submeshes.size());

		for (const SubmeshBuildData& submeshBD : m_submeshes)
		{
			rhi::BLASDefinition submeshBLAS;
			submeshBLAS.trianglesGeometry.vertexLocationsAddress = geometryDataDeviceAddress + static_cast<Uint64>(submeshBD.submesh.locationsOffset);
			submeshBLAS.trianglesGeometry.vertexLocationsStride = sizeof(math::Vector3f);
			submeshBLAS.trianglesGeometry.maxVerticesNum = submeshBD.vertexCount;
			submeshBLAS.trianglesGeometry.indicesAddress = geometryDataDeviceAddress + static_cast<Uint64>(submeshBD.submesh.indicesOffset);
			submeshBLAS.trianglesGeometry.indicesNum = submeshBD.submesh.indicesNum;

			blasesComp.blases.emplace_back(GetParameters().blasBuilder->CreateBLAS(RENDERER_RESOURCE_NAME("Temp BLAS Name"), submeshBLAS));
		}

		staticMeshDataHandle.emplace<BLASesComponent>(std::move(blasesComp));
	}

	return staticMeshDataHandle;
}

const MeshBuildParameters& MeshBuilder::GetParameters() const
{
	return m_parameters;
}

void MeshBuilder::BeginNewSubmesh()
{
	SubmeshBuildData& submeshBD = m_submeshes.emplace_back();
	submeshBD.submesh.indicesOffset					= idxNone<Uint32>;
	submeshBD.submesh.locationsOffset				= idxNone<Uint32>;
	submeshBD.submesh.normalsOffset					= idxNone<Uint32>;
	submeshBD.submesh.tangentsOffset				= idxNone<Uint32>;
	submeshBD.submesh.uvsOffset						= idxNone<Uint32>;
	submeshBD.submesh.meshletsPrimitivesDataOffset	= idxNone<Uint32>;
	submeshBD.submesh.meshletsVerticesDataOffset	= idxNone<Uint32>;
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

#if SPT_DEBUG
void MeshBuilder::ValidateSubmesh(const SubmeshBuildData& submeshBuildData) const
{
	SPT_CHECK(submeshBuildData.vertexCount > 0);
	SPT_CHECK(submeshBuildData.submesh.indicesOffset != idxNone<Uint32>);
	SPT_CHECK(submeshBuildData.submesh.locationsOffset != idxNone<Uint32>);
}
#endif // SPT_DEBUG

void MeshBuilder::ComputeBoundingSphere(SubmeshBuildData& submeshBuildData) const
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(submeshBuildData.vertexCount > 0);
	
	const math::Vector3f* locations = reinterpret_cast<const math::Vector3f*>(m_geometryData.data() + submeshBuildData.submesh.locationsOffset);

	math::Vector3f min = math::Vector3f::Ones() * 999999.f;
	math::Vector3f max = math::Vector3f::Ones() * -999999.f;

	for (Uint32 idx = 0; idx < submeshBuildData.vertexCount; ++idx)
	{
		min.x() = std::min(min.x(), locations[idx].x());
		min.y() = std::min(min.y(), locations[idx].y());
		min.z() = std::min(min.z(), locations[idx].z());

		max.x() = std::max(max.x(), locations[idx].x());
		max.y() = std::max(max.y(), locations[idx].y());
		max.z() = std::max(max.z(), locations[idx].z());
	}

	const math::Vector3f center = 0.5f * (min + max);

	Real32 radiusSq = 0.f;
	for (Uint32 idx = 0; idx < submeshBuildData.vertexCount; ++idx)
	{
		radiusSq = std::max(radiusSq, (locations[idx] - center).squaredNorm());
	}

	const Real32 radius = std::sqrt(radiusSq);
	
	submeshBuildData.submesh.boundingSphereCenter = center;
	submeshBuildData.submesh.boundingSphereRadius = radius;
}

void MeshBuilder::OptimizeSubmesh(SubmeshBuildData& submeshBuildData)
{
	SPT_PROFILER_FUNCTION();

	Uint32* indices = GetGeometryDataMutable<Uint32>(submeshBuildData.submesh.indicesOffset);

	SizeType vertexCount = static_cast<SizeType>(submeshBuildData.vertexCount);
	const SizeType indexCount = static_cast<SizeType>(submeshBuildData.submesh.indicesNum);

	meshopt_optimizeVertexCache<Uint32>(indices, indices, indexCount, vertexCount);

	lib::DynamicArray<unsigned int> remapArray(vertexCount);
	meshopt_optimizeVertexFetchRemap(remapArray.data(), indices, indexCount, vertexCount);
	meshopt_remapIndexBuffer(indices, indices, indexCount, remapArray.data());

	math::Vector3f* locations = GetGeometryDataMutable<math::Vector3f>(submeshBuildData.submesh.locationsOffset);
	meshopt_remapVertexBuffer(locations, locations, vertexCount, sizeof(math::Vector3f), remapArray.data());

	if (submeshBuildData.submesh.uvsOffset != idxNone<Uint32>)
	{
		math::Vector2f* uvs = GetGeometryDataMutable<math::Vector2f>(submeshBuildData.submesh.uvsOffset);
		meshopt_remapVertexBuffer(uvs, uvs, vertexCount, sizeof(math::Vector2f), remapArray.data());
	}

	if (submeshBuildData.submesh.normalsOffset != idxNone<Uint32>)
	{
		math::Vector3f* normals = GetGeometryDataMutable<math::Vector3f>(submeshBuildData.submesh.normalsOffset);
		meshopt_remapVertexBuffer(normals, normals, vertexCount, sizeof(math::Vector3f), remapArray.data());
	}

	if (submeshBuildData.submesh.tangentsOffset != idxNone<Uint32>)
	{
		math::Vector4f* tangents = GetGeometryDataMutable<math::Vector4f>(submeshBuildData.submesh.tangentsOffset);
		meshopt_remapVertexBuffer(tangents, tangents, vertexCount, sizeof(math::Vector4f), remapArray.data());
	}

	submeshBuildData.vertexCount = static_cast<Uint32>(vertexCount);
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

	const Uint32* indices = GetGeometryData<Uint32>(submeshBuildData.submesh.indicesOffset);
	const math::Vector3f* locations = GetGeometryData<math::Vector3f>(submeshBuildData.submesh.locationsOffset);

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

	const SizeType meshletVertsOffset = AppendData<Uint32, Uint32>(reinterpret_cast<const unsigned char*>(meshletVertices.data()), 1, sizeof(Uint32), meshletVertices.size());
	submeshBuildData.submesh.meshletsVerticesDataOffset = static_cast<Uint32>(meshletVertsOffset);

	const SizeType meshletPrimsOffset = AppendData<Uint8, Uint8>(reinterpret_cast<const unsigned char*>(meshletPrimitives.data()), 1, sizeof(Uint8), meshletPrimitives.size());
	submeshBuildData.submesh.meshletsPrimitivesDataOffset = static_cast<Uint32>(meshletPrimsOffset);

	const SizeType submeshMeshletsBegin = m_meshlets.size();
	submeshBuildData.submesh.meshletsBeginIdx = static_cast<Uint32>(submeshMeshletsBegin);
	submeshBuildData.submesh.meshletsNum = static_cast<Uint32>(finalMeshletsNum);

	m_meshlets.resize(m_meshlets.size() + finalMeshletsNum);
	for (SizeType meshletIdx = 0; meshletIdx < finalMeshletsNum; ++meshletIdx)
	{
		const SizeType meshletBuildDataIdx = submeshMeshletsBegin + meshletIdx;
		m_meshlets[meshletBuildDataIdx].triangleCount = moMeshlets[meshletIdx].triangle_count;
		m_meshlets[meshletBuildDataIdx].meshletPrimitivesOffset = (moMeshlets[meshletIdx].triangle_offset);
		m_meshlets[meshletBuildDataIdx].meshletVerticesOffset = moMeshlets[meshletIdx].vertex_offset * 4;
	}

	BuildMeshletsCullingData(submeshBuildData);
}

void MeshBuilder::BuildMeshletsCullingData(SubmeshBuildData& submeshBuildData)
{
	SPT_PROFILER_FUNCTION();

	const Real32* vertexLocations = GetGeometryData<Real32>(submeshBuildData.submesh.locationsOffset);
	const SizeType verticesNum = static_cast<SizeType>(submeshBuildData.vertexCount);

	const Uint32 firstMeshletIdx = submeshBuildData.submesh.meshletsBeginIdx;
	const Uint32 meshletsNum = submeshBuildData.submesh.meshletsNum;
	const Uint32 meshletsEndIdx = firstMeshletIdx + meshletsNum;

	for (SizeType meshletIdx = firstMeshletIdx; meshletIdx < meshletsEndIdx; ++meshletIdx)
	{
		MeshletGPUData& meshlet = m_meshlets[meshletIdx];

		const Uint32 meshletVerticesOffset = submeshBuildData.submesh.meshletsVerticesDataOffset + meshlet.meshletVerticesOffset;
		const Uint32 meshletPrimitivesOffset = submeshBuildData.submesh.meshletsPrimitivesDataOffset + meshlet.meshletPrimitivesOffset;

		const Uint32* meshletVertices = GetGeometryData<Uint32>(meshletVerticesOffset);
		const Uint8* meshletPrimitives = GetGeometryData<Uint8>(meshletPrimitivesOffset);

		const Uint32 triangleCount = meshlet.triangleCount;

		const meshopt_Bounds meshletBounds = meshopt_computeMeshletBounds(meshletVertices,
																		  meshletPrimitives,
																		  triangleCount,
																		  vertexLocations,
																		  verticesNum,
																		  sizeof(math::Vector3f));

		meshlet.boundingSphereCenter = math::Map<const math::Vector3f>(meshletBounds.center);
		meshlet.boundingSphereRadius = meshletBounds.radius;

		Int8 packedConeAxis[4];
		packedConeAxis[0] = meshletBounds.cone_axis_s8[2];
		packedConeAxis[1] = meshletBounds.cone_axis_s8[1];
		packedConeAxis[2] = meshletBounds.cone_axis_s8[0];
		packedConeAxis[3] = meshletBounds.cone_cutoff_s8;
		
		std::memcpy(&meshlet.packedConeAxisAndCutoff, &packedConeAxis, sizeof(Uint32));
	}
}

void MeshBuilder::ComputeMeshBoundingSphere()
{
	SPT_PROFILER_FUNCTION();

	math::Vector3f min = math::Vector3f::Ones() * 999999.f;
	math::Vector3f max = math::Vector3f::Ones() * -999999.f;

	for(const SubmeshBuildData& submeshBD : m_submeshes)
	{
		min.x() = std::min(min.x(), submeshBD.submesh.boundingSphereCenter.x());
		min.y() = std::min(min.y(), submeshBD.submesh.boundingSphereCenter.y());
		min.z() = std::min(min.z(), submeshBD.submesh.boundingSphereCenter.z());

		max.x() = std::max(max.x(), submeshBD.submesh.boundingSphereCenter.x());
		max.y() = std::max(max.y(), submeshBD.submesh.boundingSphereCenter.y());
		max.z() = std::max(max.z(), submeshBD.submesh.boundingSphereCenter.z());
	}

	const math::Vector3f center = 0.5f * (min + max);

	Real32 radius = 0.f;

	for(const SubmeshBuildData& submeshBD : m_submeshes)
	{
		const Real32 distToSubmesh = (submeshBD.submesh.boundingSphereCenter - center).norm();
		radius = std::max(radius, distToSubmesh + submeshBD.submesh.boundingSphereRadius);
	}

	m_boundingSphereCenter = center;
	m_boundingSphereRadius = radius;
}

Byte* MeshBuilder::AppendData(Uint64 appendSize)
{
	const SizeType prevSize = m_geometryData.size();
	const SizeType newSize = m_geometryData.size() + appendSize;
	m_geometryData.resize(newSize);
	return &m_geometryData[prevSize];
}

} // spt::rsc
