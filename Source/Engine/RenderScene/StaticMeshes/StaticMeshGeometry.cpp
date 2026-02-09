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

	SPT_STATIC_CHECK(sizeof(rdr::HLSLStorage<SubmeshGPUData>) % 4 == 0);
	SPT_STATIC_CHECK(sizeof(rdr::HLSLStorage<MeshletGPUData>) % 4 == 0);

	const Uint64 meshletsDataSize	= meshlets.size() * sizeof(rdr::HLSLStorage<MeshletGPUData>);
	const Uint64 submeshesDataSize	= submeshes.size() * sizeof(rdr::HLSLStorage<SubmeshGPUData>);

	const rhi::VirtualAllocationDefinition meshletsSuballocationDef(meshletsDataSize, sizeof(rdr::HLSLStorage<MeshletGPUData>), rhi::EVirtualAllocationFlags::PreferMinMemory);
	const rhi::RHIVirtualAllocation meshletsSuballocation = m_meshletsBuffer->GetRHI().CreateSuballocation(meshletsSuballocationDef);

	const Uint32 meshletsAllocationOffset = static_cast<Uint32>(meshletsSuballocation.GetOffset());

	// Path offsets
	std::for_each(std::begin(submeshes), std::end(submeshes),
				  [meshletsAllocationOffset, geometryDataSuballocation](SubmeshGPUData& submesh)
				  {
					  submesh.meshlets			= MeshletsGPUSpan(submesh.meshlets.GetOffset() + meshletsAllocationOffset, submesh.meshlets.GetSize());
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

	const rhi::VirtualAllocationDefinition submeshesSuballocationDef(submeshesDataSize, sizeof(rdr::HLSLStorage<SubmeshGPUData>), rhi::EVirtualAllocationFlags::PreferMinMemory);
	const rhi::RHIVirtualAllocation submeshesSuballocation = m_submeshesBuffer->GetRHI().CreateSuballocation(submeshesSuballocationDef);

	lib::DynamicArray<rdr::HLSLStorage<MeshletGPUData>> hlslMeshlets(meshlets.size());
	lib::DynamicArray<rdr::HLSLStorage<SubmeshGPUData>> hlslSubmeshes(submeshes.size());

	for (SizeType meshletIdx = 0; meshletIdx < meshlets.size(); ++meshletIdx)
	{
		hlslMeshlets[meshletIdx] = meshlets[meshletIdx];
	}

	for (SizeType submeshIdx = 0; submeshIdx < submeshes.size(); ++submeshIdx)
	{
		hlslSubmeshes[submeshIdx] = submeshes[submeshIdx];
	}

	gfx::UploadDataToBuffer(lib::Ref(m_meshletsBuffer),		meshletsSuballocation.GetOffset(),		reinterpret_cast<const Byte*>(hlslMeshlets.data()),		meshletsDataSize);
	gfx::UploadDataToBuffer(lib::Ref(m_submeshesBuffer),	submeshesSuballocation.GetOffset(),		reinterpret_cast<const Byte*>(hlslSubmeshes.data()),	submeshesDataSize);

	StaticMeshGeometryData geometryData;
	geometryData.submeshesSuballocation		= submeshesSuballocation;
	geometryData.meshletsSuballocation		= meshletsSuballocation;
	geometryData.geometrySuballocation		= geometryDataSuballocation;

	return geometryData;
}

StaticMeshGeometryBuffers StaticMeshUnifiedData::GetGeometryBuffers() const
{
	StaticMeshGeometryBuffers buffers;
	buffers.submeshesArray = m_submeshesBuffer->GetFullViewRef();
	buffers.meshletsArray  = m_meshletsBuffer->GetFullViewRef();
	return buffers;
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

	m_submeshesBuffer		= createBufferImpl(RENDERER_RESOURCE_NAME("Static Meshes Submeshes Buffer"), 1024 * 8 * sizeof(SubmeshGPUData));
	m_meshletsBuffer		= createBufferImpl(RENDERER_RESOURCE_NAME("Static Meshes Meshlets Buffer"), 1024 * 512 * sizeof(MeshletGPUData));

	rdr::Renderer::GetOnRendererCleanupDelegate().AddLambda([this]
															{
																DestroyResources();
															});
}

void StaticMeshUnifiedData::DestroyResources()
{
	m_submeshesBuffer.reset();
	m_meshletsBuffer.reset();
}

} // spt::rsc
