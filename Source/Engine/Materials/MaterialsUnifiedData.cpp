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

rhi::RHISuballocation MaterialsUnifiedData::CreateMaterialDataSuballocation(Uint32 dataSize)
{
	SPT_PROFILER_FUNCTION();

	const rhi::SuballocationDefinition suballocationDef(dataSize, 4, rhi::EBufferSuballocationFlags::PreferMinMemory);

	return m_materialsUnifiedBuffer->GetRHI().CreateSuballocation(suballocationDef);
}

rhi::RHISuballocation MaterialsUnifiedData::CreateMaterialDataSuballocation(const Byte* materialData, Uint32 dataSize)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RHISuballocation suballocation = CreateMaterialDataSuballocation(dataSize);
	SPT_CHECK(suballocation.IsValid());

	gfx::UploadDataToBuffer(lib::Ref(m_materialsUnifiedBuffer), suballocation.GetOffset(), materialData, dataSize);
	
	return suballocation;
}

lib::SharedRef<MaterialsDS> MaterialsUnifiedData::GetMaterialsDS() const
{
	return lib::Ref(m_materialsDS);
}

MaterialsUnifiedData::MaterialsUnifiedData()
	: m_materialsUnifiedBuffer(CreateMaterialsUnifiedBuffer())
	, m_materialsDS(CreateMaterialsDS())
{
	rdr::Renderer::GetOnRendererCleanupDelegate().AddLambda([this]
															{
																m_materialsUnifiedBuffer.reset();
																m_materialsDS.reset();
															});
}

lib::SharedRef<rdr::Buffer> MaterialsUnifiedData::CreateMaterialsUnifiedBuffer() const
{
	const rhi::RHIAllocationInfo allocationInfo(rhi::EMemoryUsage::GPUOnly);

	const rhi::BufferDefinition bufferDefinition(1024 * 1024, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst), rhi::EBufferFlags::WithVirtualSuballocations);

	return rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("MaterialsUnifiedData"), bufferDefinition, allocationInfo);
}

lib::SharedRef<MaterialsDS> MaterialsUnifiedData::CreateMaterialsDS() const
{
	const lib::SharedRef<MaterialsDS> ds = rdr::ResourcesManager::CreateDescriptorSetState<MaterialsDS>(RENDERER_RESOURCE_NAME("MaterialsDS"), rdr::EDescriptorSetStateFlags::Persistent);

	ds->u_materialsData = m_materialsUnifiedBuffer->CreateFullView();

	return ds;
}

} // spt::mat
