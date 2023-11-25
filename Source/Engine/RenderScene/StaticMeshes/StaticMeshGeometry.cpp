#include "StaticMeshGeometry.h"
#include "Transfers/UploadUtils.h"
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

StaticMeshGeometryData StaticMeshUnifiedData::BuildStaticMeshData(lib::DynamicArray<SubmeshGPUData>& submeshes, lib::DynamicArray<MeshletGPUData>& meshlets, rhi::RHIVirtualAllocation geometryDataSuballocation)
{
	SPT_PROFILER_FUNCTION();

	SPT_STATIC_CHECK(sizeof(SubmeshGPUData)		% 4 == 0);
	SPT_STATIC_CHECK(sizeof(MeshletGPUData)		% 4 == 0);

	const Uint64 meshletsDataSize	= meshlets.size() * sizeof(MeshletGPUData);
	const Uint64 submeshesDataSize	= submeshes.size() * sizeof(SubmeshGPUData);

	const rhi::VirtualAllocationDefinition meshletsSuballocationDef(meshletsDataSize, sizeof(MeshletGPUData), rhi::EVirtualAllocationFlags::PreferMinMemory);
	const rhi::RHIVirtualAllocation meshletsSuballocation = m_meshletsBuffer->GetRHI().CreateSuballocation(meshletsSuballocationDef);

	const SizeType meshletsAllocationStartIdx = meshletsSuballocation.GetOffset() / sizeof(MeshletGPUData);

	std::for_each(std::begin(submeshes), std::end(submeshes),
				  [meshletsAllocationStartIdx, geometryDataSuballocation](SubmeshGPUData& submesh)
				  {
					  submesh.meshletsBeginIdx	+= static_cast<Uint32>(meshletsAllocationStartIdx);
					  submesh.indicesOffset		+= static_cast<Uint32>(geometryDataSuballocation.GetOffset());
					  submesh.locationsOffset	+= static_cast<Uint32>(geometryDataSuballocation.GetOffset());
					  if (submesh.normalsOffset != idxNone<Uint32>)
					  {
						  submesh.normalsOffset += static_cast<Uint32>(geometryDataSuballocation.GetOffset());
					  }
					  if (submesh.tangentsOffset != idxNone<Uint32>)
					  {
						  submesh.tangentsOffset += static_cast<Uint32>(geometryDataSuballocation.GetOffset());
					  }
					  if (submesh.uvsOffset != idxNone<Uint32>)
					  {
						  submesh.uvsOffset += static_cast<Uint32>(geometryDataSuballocation.GetOffset());
					  }
					  if (submesh.meshletsPrimitivesDataOffset != idxNone<Uint32>)
					  {
						  submesh.meshletsPrimitivesDataOffset += static_cast<Uint32>(geometryDataSuballocation.GetOffset());
					  }
					  if (submesh.meshletsVerticesDataOffset != idxNone<Uint32>)
					  {
						  submesh.meshletsVerticesDataOffset += static_cast<Uint32>(geometryDataSuballocation.GetOffset());
					  }
				  });

	const rhi::VirtualAllocationDefinition submeshesSuballocationDef(submeshesDataSize, sizeof(SubmeshGPUData), rhi::EVirtualAllocationFlags::PreferMinMemory);
	const rhi::RHIVirtualAllocation submeshesSuballocation = m_submeshesBuffer->GetRHI().CreateSuballocation(submeshesSuballocationDef);

	gfx::UploadDataToBuffer(lib::Ref(m_meshletsBuffer),		meshletsSuballocation.GetOffset(),		reinterpret_cast<const Byte*>(meshlets.data()),		meshletsDataSize);
	gfx::UploadDataToBuffer(lib::Ref(m_submeshesBuffer),	submeshesSuballocation.GetOffset(),		reinterpret_cast<const Byte*>(submeshes.data()),	submeshesDataSize);

	StaticMeshGeometryData geometryData;
	geometryData.submeshesSuballocation		= submeshesSuballocation;
	geometryData.meshletsSuballocation		= meshletsSuballocation;
	geometryData.geometrySuballocation		= geometryDataSuballocation;

	return geometryData;
}

const lib::MTHandle<StaticMeshUnifiedDataDS>& StaticMeshUnifiedData::GetUnifiedDataDS() const
{
	return m_unifiedDataDS;
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

	m_submeshesBuffer		= createBufferImpl(RENDERER_RESOURCE_NAME("Static Meshes Submeshes Buffer"), 1024 * 32 * sizeof(SubmeshGPUData));
	m_meshletsBuffer		= createBufferImpl(RENDERER_RESOURCE_NAME("Static Meshes Meshlets Buffer"), 1024 * 512 * sizeof(MeshletGPUData));

	m_unifiedDataDS = rdr::ResourcesManager::CreateDescriptorSetState<StaticMeshUnifiedDataDS>(RENDERER_RESOURCE_NAME("Static Mesh Unified Data DS"), rdr::EDescriptorSetStateFlags::Persistent);
	m_unifiedDataDS->u_submeshes	= m_submeshesBuffer->CreateFullView();
	m_unifiedDataDS->u_meshlets		= m_meshletsBuffer->CreateFullView();

	rdr::Renderer::GetOnRendererCleanupDelegate().AddLambda([this]
															{
																DestroyResources();
															});
}

void StaticMeshUnifiedData::DestroyResources()
{
	m_submeshesBuffer.reset();
	m_meshletsBuffer.reset();
	m_unifiedDataDS.Reset();
}

} // spt::rsc
