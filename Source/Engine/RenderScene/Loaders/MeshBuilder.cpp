#include "MeshBuilder.h"
#include "StaticMeshes/StaticMeshRenderSceneSubsystem.h"
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

lib::MTHandle<RenderMesh> MeshBuilder::EmitMeshGeometry()
{
	SPT_PROFILER_FUNCTION();

	Build();

	lib::MTHandle<RenderMesh> renderMesh = new RenderMesh();
	renderMesh->Initialize(CreateMeshDefinition());

	if (GetParameters().blasBuilder)
	{
		renderMesh->BuildBLASes(*GetParameters().blasBuilder);
	}

	return renderMesh;
}

void MeshBuilder::Build()
{
	SPT_PROFILER_FUNCTION();

	for (SubmeshDefinition& submesh : m_submeshes)
	{
#if SPT_DEBUG
		ValidateSubmesh(submesh);
#endif // SPT_DEBUG

		if (GetParameters().optimizeMesh)
		{
			OptimizeSubmesh(submesh);
		}

		BuildMeshlets(submesh);

		ComputeBoundingSphere(submesh);
	}

	ComputeMeshBoundingSphere();
}

MeshDefinition MeshBuilder::CreateMeshDefinition() const
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!m_submeshes.empty());
	SPT_CHECK(!m_meshlets.empty());

	MeshDefinition meshDef;
	meshDef.meshletsOffset  = sizeof(SubmeshDefinition) * static_cast<Uint32>(m_submeshes.size());
	meshDef.geometryOffset  = meshDef.meshletsOffset + sizeof(MeshletDefinition) * static_cast<Uint32>(m_meshlets.size());

	meshDef.boundingSphereCenter = m_boundingSphereCenter;
	meshDef.boundingSphereRadius = m_boundingSphereRadius;

	const SizeType totalSize = meshDef.geometryOffset + m_geometryData.size();

	meshDef.blob.resize(totalSize);
	meshDef.blobView = lib::Span<const Byte>(meshDef.blob.data(), meshDef.blob.size());

	// Write Submeshes
	{
		const Uint32 submeshesOffset = 0u;
		SubmeshDefinition* submeshesDst = reinterpret_cast<SubmeshDefinition*>(&meshDef.blob[submeshesOffset]);
		std::memcpy(submeshesDst, m_submeshes.data(), sizeof(SubmeshDefinition) * m_submeshes.size());
	}

	// Write Meshlets
	{
		const Uint32 meshletsOffset = meshDef.meshletsOffset;
		MeshletDefinition* meshletsDst = reinterpret_cast<MeshletDefinition*>(&meshDef.blob[meshletsOffset]);
		std::memcpy(meshletsDst, m_meshlets.data(), sizeof(MeshletDefinition) * m_meshlets.size());
	}

	// Write Geometry Data
	{
		const Uint32 geometryOffset = meshDef.geometryOffset;
		Byte* geometryDataDst = &meshDef.blob[geometryOffset];
		std::memcpy(geometryDataDst, m_geometryData.data(), m_geometryData.size());
	}

	return meshDef;
}

const MeshBuildParameters& MeshBuilder::GetParameters() const
{
	return m_parameters;
}

void MeshBuilder::BeginNewSubmesh()
{
	SubmeshDefinition& submesh = m_submeshes.emplace_back();
	submesh.indicesOffset                = idxNone<Uint32>;
	submesh.locationsOffset              = idxNone<Uint32>;
	submesh.normalsOffset                = idxNone<Uint32>;
	submesh.tangentsOffset               = idxNone<Uint32>;
	submesh.uvsOffset                    = idxNone<Uint32>;
	submesh.meshletsPrimitivesDataOffset = idxNone<Uint32>;
	submesh.meshletsVerticesDataOffset   = idxNone<Uint32>;
}

SubmeshDefinition& MeshBuilder::GetCurrentSubmesh()
{
	SPT_CHECK(!m_submeshes.empty());
	return m_submeshes.back();
}

Uint64 MeshBuilder::GetCurrentDataSize() const
{
	return m_geometryData.size();
}

#if SPT_DEBUG
void MeshBuilder::ValidateSubmesh(SubmeshDefinition& submesh) const
{
	SPT_CHECK(submesh.vertexCount > 0);
	SPT_CHECK(submesh.indicesOffset != idxNone<Uint32>);
	SPT_CHECK(submesh.locationsOffset != idxNone<Uint32>);
}
#endif // SPT_DEBUG

void MeshBuilder::ComputeBoundingSphere(SubmeshDefinition& submesh) const
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(submesh.verticesNum > 0);
	
	const math::Vector3f* locations = reinterpret_cast<const math::Vector3f*>(m_geometryData.data() + submesh.locationsOffset);

	math::Vector3f min = math::Vector3f::Ones() * 999999.f;
	math::Vector3f max = math::Vector3f::Ones() * -999999.f;

	for (Uint32 idx = 0; idx < submesh.verticesNum; ++idx)
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
	for (Uint32 idx = 0; idx < submesh.verticesNum; ++idx)
	{
		radiusSq = std::max(radiusSq, (locations[idx] - center).squaredNorm());
	}

	const Real32 radius = std::sqrt(radiusSq);
	
	submesh.boundingSphereCenter = center;
	submesh.boundingSphereRadius = radius;
}

void MeshBuilder::OptimizeSubmesh(SubmeshDefinition& submesh)
{
	SPT_PROFILER_FUNCTION();

	Uint32* indices = GetGeometryDataMutable<Uint32>(submesh.indicesOffset);

	SizeType vertexCount = static_cast<SizeType>(submesh.verticesNum);
	const SizeType indexCount = static_cast<SizeType>(submesh.indicesNum);

	meshopt_optimizeVertexCache<Uint32>(indices, indices, indexCount, vertexCount);

	lib::DynamicArray<unsigned int> remapArray(vertexCount);
	meshopt_optimizeVertexFetchRemap(remapArray.data(), indices, indexCount, vertexCount);
	meshopt_remapIndexBuffer(indices, indices, indexCount, remapArray.data());

	math::Vector3f* locations = GetGeometryDataMutable<math::Vector3f>(submesh.locationsOffset);
	meshopt_remapVertexBuffer(locations, locations, vertexCount, sizeof(math::Vector3f), remapArray.data());

	if (submesh.uvsOffset != idxNone<Uint32>)
	{
		math::Vector2f* uvs = GetGeometryDataMutable<math::Vector2f>(submesh.uvsOffset);
		meshopt_remapVertexBuffer(uvs, uvs, vertexCount, sizeof(math::Vector2f), remapArray.data());
	}

	if (submesh.normalsOffset != idxNone<Uint32>)
	{
		math::Vector3f* normals = GetGeometryDataMutable<math::Vector3f>(submesh.normalsOffset);
		meshopt_remapVertexBuffer(normals, normals, vertexCount, sizeof(math::Vector3f), remapArray.data());
	}

	if (submesh.tangentsOffset != idxNone<Uint32>)
	{
		math::Vector4f* tangents = GetGeometryDataMutable<math::Vector4f>(submesh.tangentsOffset);
		meshopt_remapVertexBuffer(tangents, tangents, vertexCount, sizeof(math::Vector4f), remapArray.data());
	}

	submesh.verticesNum = static_cast<Uint32>(vertexCount);
}

void MeshBuilder::BuildMeshlets(SubmeshDefinition& submesh)
{
	SPT_PROFILER_FUNCTION();

	const SizeType meshletMaxTriangles = 64;
	const SizeType meshletMaxVertices = 64;

	const SizeType vertexCount = static_cast<SizeType>(submesh.verticesNum);
	const SizeType indexCount = static_cast<SizeType>(submesh.indicesNum);

	const SizeType meshletsMaxNum = meshopt_buildMeshletsBound(indexCount, meshletMaxVertices, meshletMaxTriangles);

	lib::DynamicArray<Uint32> meshletVertices(meshletsMaxNum * meshletMaxVertices);
	lib::DynamicArray<Uint8> meshletPrimitives(meshletsMaxNum * meshletMaxTriangles * 3);

	lib::DynamicArray<meshopt_Meshlet> moMeshlets(meshletsMaxNum);

	const Uint32* indices = GetGeometryData<Uint32>(submesh.indicesOffset);
	const math::Vector3f* locations = GetGeometryData<math::Vector3f>(submesh.locationsOffset);

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
	submesh.meshletsVerticesDataOffset = static_cast<Uint32>(meshletVertsOffset);

	const SizeType meshletPrimsOffset = AppendData<Uint8, Uint8>(reinterpret_cast<const unsigned char*>(meshletPrimitives.data()), 1, sizeof(Uint8), meshletPrimitives.size());
	submesh.meshletsPrimitivesDataOffset = static_cast<Uint32>(meshletPrimsOffset);

	const SizeType submeshMeshletsBegin = m_meshlets.size();
	submesh.firstMeshletIdx = static_cast<Uint32>(submeshMeshletsBegin);
	submesh.meshletsNum     = static_cast<Uint32>(finalMeshletsNum);

	m_meshlets.resize(m_meshlets.size() + finalMeshletsNum);
	for (SizeType meshletIdx = 0; meshletIdx < finalMeshletsNum; ++meshletIdx)
	{
		const SizeType meshletBuildDataIdx = submeshMeshletsBegin + meshletIdx;
		m_meshlets[meshletBuildDataIdx].triangleCount           = static_cast<Uint16>(moMeshlets[meshletIdx].triangle_count);
		m_meshlets[meshletBuildDataIdx].vertexCount             = static_cast<Uint16>(moMeshlets[meshletIdx].vertex_count);
		m_meshlets[meshletBuildDataIdx].meshletPrimitivesOffset = moMeshlets[meshletIdx].triangle_offset;
		m_meshlets[meshletBuildDataIdx].meshletVerticesOffset   = moMeshlets[meshletIdx].vertex_offset * 4;
	}

	BuildMeshletsCullingData(submesh);
}

void MeshBuilder::BuildMeshletsCullingData(SubmeshDefinition& submesh)
{
	SPT_PROFILER_FUNCTION();

	const Real32* vertexLocations = GetGeometryData<Real32>(submesh.locationsOffset);
	const SizeType verticesNum = static_cast<SizeType>(submesh.verticesNum);

	const Uint32 firstMeshletIdx = submesh.firstMeshletIdx;
	const Uint32 meshletsNum = submesh.meshletsNum;
	const Uint32 meshletsEndIdx = firstMeshletIdx + meshletsNum;

	for (SizeType meshletIdx = firstMeshletIdx; meshletIdx < meshletsEndIdx; ++meshletIdx)
	{
		MeshletDefinition& meshlet = m_meshlets[meshletIdx];

		const Uint32 meshletVerticesOffset   = submesh.meshletsVerticesDataOffset + meshlet.meshletVerticesOffset;
		const Uint32 meshletPrimitivesOffset = submesh.meshletsPrimitivesDataOffset + meshlet.meshletPrimitivesOffset;

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

	for(const SubmeshDefinition& submesh : m_submeshes)
	{
		min.x() = std::min(min.x(), submesh.boundingSphereCenter.x());
		min.y() = std::min(min.y(), submesh.boundingSphereCenter.y());
		min.z() = std::min(min.z(), submesh.boundingSphereCenter.z());

		max.x() = std::max(max.x(), submesh.boundingSphereCenter.x());
		max.y() = std::max(max.y(), submesh.boundingSphereCenter.y());
		max.z() = std::max(max.z(), submesh.boundingSphereCenter.z());
	}

	const math::Vector3f center = 0.5f * (min + max);

	Real32 radius = 0.f;

	for(const SubmeshDefinition& submesh : m_submeshes)
	{
		const Real32 distToSubmesh = (submesh.boundingSphereCenter - center).norm();
		radius = std::max(radius, distToSubmesh + submesh.boundingSphereRadius);
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
