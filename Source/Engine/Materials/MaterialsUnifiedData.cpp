#include "MaterialsUnifiedData.h"
#include "ResourcesManager.h"
#include "Transfers/UploadUtils.h"
#include "Renderer.h"

namespace spt::mat
{

MaterialsUnifiedData& MaterialsUnifiedData::Get()
{
	static MaterialsUnifiedData instance;
	return instance;
}

Uint32 MaterialsUnifiedData::AddMaterialTexture(const lib::SharedRef<rdr::TextureView>& textureView)
{
	SPT_PROFILER_FUNCTION();

	return m_materialsDS->u_materialsTextures.BindTexture(textureView);
}

rhi::RHIVirtualAllocation MaterialsUnifiedData::CreateMaterialDataSuballocation(Uint64 dataSize)
{
	SPT_PROFILER_FUNCTION();

	const rhi::VirtualAllocationDefinition virtualAllocationDef(dataSize, 4, rhi::EVirtualAllocationFlags::PreferMinMemory);

	return m_materialsUnifiedBuffer->GetRHI().CreateSuballocation(virtualAllocationDef);
}

rhi::RHIVirtualAllocation MaterialsUnifiedData::CreateMaterialDataSuballocation(const Byte* materialData, Uint64 dataSize)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RHIVirtualAllocation suballocation = CreateMaterialDataSuballocation(dataSize);
	SPT_CHECK(suballocation.IsValid());

	gfx::UploadDataToBuffer(lib::Ref(m_materialsUnifiedBuffer), suballocation.GetOffset(), materialData, dataSize);
	
	return suballocation;
}

lib::MTHandle<MaterialsDS> MaterialsUnifiedData::GetMaterialsDS() const
{
	return m_materialsDS;
}

MaterialsUnifiedData::MaterialsUnifiedData()
	: m_materialsUnifiedBuffer(CreateMaterialsUnifiedBuffer())
	, m_materialsDS(CreateMaterialsDS())
{
	rdr::Renderer::GetOnRendererCleanupDelegate().AddLambda([this]
															{
																m_materialsUnifiedBuffer.reset();
																m_materialsDS.Reset();
															});
}

lib::SharedRef<rdr::Buffer> MaterialsUnifiedData::CreateMaterialsUnifiedBuffer() const
{
	const rhi::RHIAllocationInfo allocationInfo(rhi::EMemoryUsage::GPUOnly);

	const rhi::BufferDefinition bufferDefinition(1024 * 1024, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst), rhi::EBufferFlags::WithVirtualSuballocations);

	return rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("MaterialsUnifiedData"), bufferDefinition, allocationInfo);
}

lib::MTHandle<MaterialsDS> MaterialsUnifiedData::CreateMaterialsDS() const
{
	const lib::MTHandle<MaterialsDS> ds = rdr::ResourcesManager::CreateDescriptorSetState<MaterialsDS>(RENDERER_RESOURCE_NAME("MaterialsDS"));

	ds->u_materialsData = m_materialsUnifiedBuffer->CreateFullView();

	return ds;
}

} // spt::mat
