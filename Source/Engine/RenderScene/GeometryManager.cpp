#include "GeometryManager.h"
#include "BufferUtilities.h"
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

rhi::RHISuballocation GeometryManager::CreatePrimitiveRenderData(const Byte* primitiveData, Uint64 dataSize)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RHISuballocation primitivesDataSuballocation = CreatePrimitiveRenderData(dataSize);

	gfx::UploadDataToBuffer(lib::Ref(m_primitivesBuffer), primitivesDataSuballocation.GetOffset(), primitiveData, dataSize);

	return primitivesDataSuballocation;
}

rhi::RHISuballocation GeometryManager::CreatePrimitiveRenderData(Uint64 dataSize)
{
	SPT_PROFILER_FUNCTION();

	const rhi::SuballocationDefinition suballocationDef(dataSize, sizeof(PrimitiveGeometryInfo), rhi::EBufferSuballocationFlags::PreferMinMemory);
	const rhi::RHISuballocation primitivesDataSuballocation = m_primitivesBuffer->GetRHI().CreateSuballocation(suballocationDef);
	SPT_CHECK(primitivesDataSuballocation.IsValid());

	return primitivesDataSuballocation;
}

GeometryManager::GeometryManager()
{
	const rhi::EBufferUsage unifiedBuffersUsage = lib::Flags(rhi::EBufferUsage::TransferDst, rhi::EBufferUsage::Storage);
	const rhi::EBufferFlags unifiedBuffersFlags = rhi::EBufferFlags::WithVirtualSuballocations;

	const rhi::RHIAllocationInfo ugbAllocationInfo(rhi::EMemoryUsage::GPUOnly);
	const rhi::BufferDefinition ugbDef(512 * 1024 * 1024, unifiedBuffersUsage, unifiedBuffersFlags);
	m_geometryBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("UnifiedGeometryBuffer"), ugbDef, ugbAllocationInfo);

	const rhi::RHIAllocationInfo upbAllocationInfo(rhi::EMemoryUsage::GPUOnly);
	const rhi::BufferDefinition upbDef(8 * 1024 * 1024, unifiedBuffersUsage, unifiedBuffersFlags);
	m_primitivesBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("UnifiedPrimitivesBuffer"), upbDef, upbAllocationInfo);

	m_geometryDSState = rdr::ResourcesManager::CreateDescriptorSetState<GeometryDS>(RENDERER_RESOURCE_NAME("UGB DS"), rdr::EDescriptorSetStateFlags::Persistent);
	m_geometryDSState->geometryData = m_geometryBuffer->CreateFullView();
	m_geometryDSState->primitivesData = m_primitivesBuffer->CreateFullView();

	m_primitivesDSState = rdr::ResourcesManager::CreateDescriptorSetState<PrimitivesDS>(RENDERER_RESOURCE_NAME("UPS DS"), rdr::EDescriptorSetStateFlags::Persistent);
	m_primitivesDSState->primitivesData = m_primitivesBuffer->CreateFullView();
	
	// This is singleton object so we can capture this safely
	rdr::Renderer::GetOnRendererCleanupDelegate().AddLambda([this]
															{
																DestroyResources();
															});
}

const lib::SharedPtr<PrimitivesDS>& GeometryManager::GetPrimitivesDSState() const
{
	return m_primitivesDSState;
}

const lib::SharedPtr<GeometryDS>& GeometryManager::GetGeometryDSState() const
{
	return m_geometryDSState;
}

void GeometryManager::DestroyResources()
{
	m_geometryBuffer.reset();
	m_primitivesBuffer.reset();
	m_geometryDSState.reset();
	m_primitivesDSState.reset();
}

} // spt::rsc
