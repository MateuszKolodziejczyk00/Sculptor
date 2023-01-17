#include "StaticMeshGeometry.h"
#include "BufferUtilities.h"
#include "RendererUtils.h"
#include "ResourcesManager.h"
#include "Renderer.h"

namespace spt::rsc
{

StaticMeshUnifiedData& StaticMeshUnifiedData::Get()
{
	static StaticMeshUnifiedData instance;
	return instance;
}

StaticMeshGeometryData StaticMeshUnifiedData::BuildStaticMeshData(lib::DynamicArray<SubmeshGPUData>& submeshes, lib::DynamicArray<MeshletGPUData>& meshlets, rhi::RHISuballocation geometryDataSuballocation)
{
	SPT_PROFILER_FUNCTION();

	SPT_STATIC_CHECK(sizeof(StaticMeshGPUData)	% 4 == 0);
	SPT_STATIC_CHECK(sizeof(SubmeshGPUData)		% 4 == 0);
	SPT_STATIC_CHECK(sizeof(MeshletGPUData)		% 4 == 0);

	const Uint64 meshletsDataSize	= meshlets.size() * sizeof(MeshletGPUData);
	const Uint64 submeshesDataSize	= submeshes.size() * sizeof(SubmeshGPUData);
	const Uint64 staticMeshDataSize = sizeof(StaticMeshGPUData);

	const rhi::SuballocationDefinition meshletsSuballocationDef(meshletsDataSize, sizeof(MeshletGPUData), rhi::EBufferSuballocationFlags::PreferMinMemory);
	const rhi::RHISuballocation meshletsSuballocation = m_meshletsBuffer->GetRHI().CreateSuballocation(meshletsSuballocationDef);

	const SizeType meshletsAllocationStartIdx = meshletsSuballocation.GetOffset() / sizeof(MeshletGPUData);

	std::for_each(std::begin(submeshes), std::end(submeshes),
				  [meshletsAllocationStartIdx](SubmeshGPUData& submesh)
				  {
					  submesh.meshletsBeginIdx += static_cast<Uint32>(meshletsAllocationStartIdx);
				  });

	const rhi::SuballocationDefinition submeshesSuballocationDef(submeshesDataSize, sizeof(SubmeshGPUData), rhi::EBufferSuballocationFlags::PreferMinMemory);
	const rhi::RHISuballocation submeshesSuballocation = m_submeshesBuffer->GetRHI().CreateSuballocation(submeshesSuballocationDef);

	const SizeType submeshesAllocationStartIdx = submeshesSuballocation.GetOffset() / sizeof(SubmeshGPUData);

	StaticMeshGPUData staticMesh;
	staticMesh.geometryDataOffset	= static_cast<Uint32>(geometryDataSuballocation.GetOffset());
	staticMesh.submeshesBeginIdx	= static_cast<Uint32>(submeshesAllocationStartIdx);
	staticMesh.submeshesNum			= static_cast<Uint32>(submeshes.size());
	
	const rhi::SuballocationDefinition staticMeshSuballocationDef(staticMeshDataSize, sizeof(StaticMeshGPUData), rhi::EBufferSuballocationFlags::PreferFasterAllocation);
	const rhi::RHISuballocation staticMeshSuballocation = m_submeshesBuffer->GetRHI().CreateSuballocation(staticMeshSuballocationDef);

	gfx::UploadDataToBuffer(lib::Ref(m_meshletsBuffer),		meshletsSuballocation.GetOffset(),		reinterpret_cast<const Byte*>(meshlets.data()),		meshletsDataSize);
	gfx::UploadDataToBuffer(lib::Ref(m_submeshesBuffer),	submeshesSuballocation.GetOffset(),		reinterpret_cast<const Byte*>(submeshes.data()),	submeshesDataSize);
	gfx::UploadDataToBuffer(lib::Ref(m_staticMeshesBuffer),	staticMeshSuballocation.GetOffset(),	reinterpret_cast<const Byte*>(&staticMesh),			staticMeshDataSize);

	StaticMeshGeometryData geometryData;
	geometryData.staticMeshSuballocation = staticMeshSuballocation;
	geometryData.submeshesSuballocation = submeshesSuballocation;
	geometryData.meshletsSuballocation = meshletsSuballocation;
	geometryData.geometrySuballocation = geometryDataSuballocation;

	return geometryData;
}

StaticMeshUnifiedData::StaticMeshUnifiedData()
{
	const auto createBufferImpl = [](const rdr::RendererResourceName& name, Uint64 size)
	{
		const rhi::EBufferUsage bufferUsage = lib::Flags(rhi::EBufferUsage::TransferDst, rhi::EBufferUsage::Storage);
		const rhi::EBufferFlags bufferFlags = rhi::EBufferFlags::WithVirtualSuballocations;
		const rhi::RHIAllocationInfo bufferAllocationInfo(rhi::EMemoryUsage::GPUOnly);

		const rhi::BufferDefinition bufferDef(size, bufferUsage, bufferFlags);
		return rdr::ResourcesManager::CreateBuffer(name, bufferDef, bufferAllocationInfo);
	};

	m_staticMeshesBuffer	= createBufferImpl(RENDERER_RESOURCE_NAME("Static Meshes Buffer"), 1024 * sizeof(StaticMeshGPUData));
	m_submeshesBuffer		= createBufferImpl(RENDERER_RESOURCE_NAME("Static Meshes Submeshes Buffer"), 1024 * 32 * sizeof(SubmeshGPUData));
	m_meshletsBuffer		= createBufferImpl(RENDERER_RESOURCE_NAME("Static Meshes Meshlets Buffer"), 1024 * 512 * sizeof(MeshletGPUData));

	rdr::Renderer::GetOnRendererCleanupDelegate().AddLambda([this]
															{
																DestroyResources();
															});
}

void StaticMeshUnifiedData::DestroyResources()
{
	m_staticMeshesBuffer.reset();
	m_submeshesBuffer.reset();
	m_meshletsBuffer.reset();
}

} // spt::rsc
