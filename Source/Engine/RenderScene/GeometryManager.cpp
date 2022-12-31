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

rhi::RHISuballocation GeometryManager::CreateGeometry(const Byte* geometryData, Uint64 size)
{
	SPT_PROFILER_FUNCTION();

	const rhi::SuballocationDefinition suballocationDef(size, 4, rhi::EBufferSuballocationFlags::PreferMinMemory);
	const rhi::RHISuballocation geometryDataSuballocation = m_geometryBuffer->GetRHI().CreateSuballocation(suballocationDef);
	SPT_CHECK(geometryDataSuballocation.IsValid());

	gfx::UploadDataToBuffer(lib::Ref(m_geometryBuffer), geometryDataSuballocation.GetOffset(), geometryData, size);

	return geometryDataSuballocation;
}

rhi::RHISuballocation GeometryManager::CreatePrimitives(const PrimitiveGeometryInfo* primitivesInfo, Uint32 primitivesNum)
{
	SPT_PROFILER_FUNCTION();

	const Uint64 primitivesDataSize = sizeof(PrimitiveGeometryInfo) * static_cast<Uint64>(primitivesNum);

	const rhi::SuballocationDefinition suballocationDef(primitivesDataSize, sizeof(PrimitiveGeometryInfo), rhi::EBufferSuballocationFlags::PreferMinMemory);
	const rhi::RHISuballocation primitivesDataSuballocation = m_primitivesBuffer->GetRHI().CreateSuballocation(suballocationDef);
	SPT_CHECK(primitivesDataSuballocation.IsValid());

	gfx::UploadDataToBuffer(lib::Ref(m_primitivesBuffer), primitivesDataSuballocation.GetOffset(), reinterpret_cast<const Byte*>(primitivesInfo), primitivesDataSize);

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
	
	// This is singleton object so we can capture this safely
	rdr::Renderer::GetOnRendererCleanupDelegate().AddLambda([this]
															{
																m_geometryBuffer.reset();
																m_primitivesBuffer.reset();
															});
}

} // spt::rsc
