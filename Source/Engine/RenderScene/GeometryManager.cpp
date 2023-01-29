#include "GeometryManager.h"
#include "Transfers/UploadUtils.h"
#include "ResourcesManager.h"
#include "Renderer.h"

namespace spt::rsc
{

GeometryManager& GeometryManager::Get()
{
	static GeometryManager instance;
	return instance;
}

rhi::RHISuballocation GeometryManager::CreateGeometry(const Byte* geometryData, Uint64 dataSize)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RHISuballocation geometryDataSuballocation = CreateGeometry(dataSize);

	gfx::UploadDataToBuffer(lib::Ref(m_geometryBuffer), geometryDataSuballocation.GetOffset(), geometryData, dataSize);

	return geometryDataSuballocation;
}

rhi::RHISuballocation GeometryManager::CreateGeometry(Uint64 dataSize)
{
	SPT_PROFILER_FUNCTION();

	const rhi::SuballocationDefinition suballocationDef(dataSize, 4, rhi::EBufferSuballocationFlags::PreferMinMemory);
	const rhi::RHISuballocation geometryDataSuballocation = m_geometryBuffer->GetRHI().CreateSuballocation(suballocationDef);
	SPT_CHECK(geometryDataSuballocation.IsValid());

	return geometryDataSuballocation;
}

GeometryManager::GeometryManager()
{
	const rhi::EBufferUsage ugbUsage = lib::Flags(rhi::EBufferUsage::TransferDst, rhi::EBufferUsage::Storage);
	const rhi::EBufferFlags ugbFlags = rhi::EBufferFlags::WithVirtualSuballocations;

	const rhi::RHIAllocationInfo ugbAllocationInfo(rhi::EMemoryUsage::GPUOnly);
	const rhi::BufferDefinition ugbDef(512 * 1024 * 1024, ugbUsage, ugbFlags);
	m_geometryBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("UnifiedGeometryBuffer"), ugbDef, ugbAllocationInfo);

	m_geometryDSState = rdr::ResourcesManager::CreateDescriptorSetState<GeometryDS>(RENDERER_RESOURCE_NAME("UGB DS"), rdr::EDescriptorSetStateFlags::Persistent);
	m_geometryDSState->u_geometryData = m_geometryBuffer->CreateFullView();
	
	// This is singleton object so we can capture this safely
	rdr::Renderer::GetOnRendererCleanupDelegate().AddLambda([this]
															{
																DestroyResources();
															});
}

const lib::SharedPtr<GeometryDS>& GeometryManager::GetGeometryDSState() const
{
	return m_geometryDSState;
}

void GeometryManager::DestroyResources()
{
	m_geometryBuffer.reset();
	m_geometryDSState.reset();
}

} // spt::rsc
