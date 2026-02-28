#include "MaterialsUnifiedData.h"
#include "ResourcesManager.h"
#include "Utils/TransfersUtils.h"
#include "GPUApi.h"
#include "MaterialTypes.h"

namespace spt::mat
{

MaterialsUnifiedData& MaterialsUnifiedData::Get()
{
	static MaterialsUnifiedData instance;
	return instance;
}

void MaterialsUnifiedData::AddMaterialTexture(const lib::SharedRef<rdr::TextureView>& textureView)
{
	SPT_PROFILER_FUNCTION();

	m_materialTextures.emplace_back(textureView);
}

rhi::RHIVirtualAllocation MaterialsUnifiedData::CreateMaterialDataSuballocation(Uint64 dataSize)
{
	SPT_PROFILER_FUNCTION();

	const rhi::VirtualAllocationDefinition virtualAllocationDef(dataSize, constants::materialDataAlignment, rhi::EVirtualAllocationFlags::PreferMinMemory);

	return m_materialsUnifiedBuffer->GetRHI().CreateSuballocation(virtualAllocationDef);
}

rhi::RHIVirtualAllocation MaterialsUnifiedData::CreateMaterialDataSuballocation(const Byte* materialData, Uint64 dataSize)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RHIVirtualAllocation suballocation = CreateMaterialDataSuballocation(dataSize);
	SPT_CHECK(suballocation.IsValid());

	rdr::UploadDataToBuffer(lib::Ref(m_materialsUnifiedBuffer), suballocation.GetOffset(), materialData, dataSize);
	
	return suballocation;
}

MaterialUnifiedData MaterialsUnifiedData::GetMaterialUnifiedData() const
{
	MaterialUnifiedData materials;
	materials.materialsData = m_materialsUnifiedBuffer->GetFullView();
	return materials;
}

MaterialsUnifiedData::MaterialsUnifiedData()
	: m_materialsUnifiedBuffer(CreateMaterialsUnifiedBuffer())
{
	rdr::GPUApi::GetOnRendererCleanupDelegate().AddLambda([this]
															{
																m_materialsUnifiedBuffer.reset();
																m_materialTextures.clear();
															});
}

lib::SharedRef<rdr::Buffer> MaterialsUnifiedData::CreateMaterialsUnifiedBuffer() const
{
	const rhi::RHIAllocationInfo allocationInfo(rhi::EMemoryUsage::GPUOnly);

	const rhi::BufferDefinition bufferDefinition(1024 * 1024, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst), rhi::EBufferFlags::WithVirtualSuballocations);

	return rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("MaterialsUnifiedData"), bufferDefinition, allocationInfo);
}

} // spt::mat
