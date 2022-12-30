#include "GeometryManager.h"
#include "BufferUtilities.h"

namespace spt::gfx
{

GeometryManager& GeometryManager::Get()
{
	static GeometryManager instance;
	return instance;
}

rhi::RHISuballocation GeometryManager::CreateGeometry(const Byte* geometryData, Uint64 size)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RHISuballocation geometryDataSuballocation = m_geometryBuffer.Allocate(size);
	SPT_CHECK(geometryDataSuballocation.IsValid());

	UploadDataToBuffer(lib::Ref(m_geometryBuffer.GetBufferInstance()), geometryDataSuballocation.GetOffset(), geometryData, size);

	return geometryDataSuballocation;
}

rhi::RHISuballocation GeometryManager::CreatePrimitives(const PrimitiveGeometryInfo* primitivesInfo, Uint32 primitivesNum)
{
	SPT_PROFILER_FUNCTION();

	const Uint64 primitivesDataSize = sizeof(PrimitiveGeometryInfo) * static_cast<Uint64>(primitivesNum);

	const rhi::RHISuballocation primitivesDataSuballocation = m_primitivesBuffer.Allocate(primitivesDataSize);
	SPT_CHECK(primitivesDataSuballocation.IsValid());

	UploadDataToBuffer(lib::Ref(m_primitivesBuffer.GetBufferInstance()), primitivesDataSuballocation.GetOffset(), reinterpret_cast<const Byte*>(primitivesInfo), primitivesDataSize);

	return primitivesDataSuballocation;
}

GeometryManager::GeometryManager()
	: m_geometryBuffer(512 * 1024 * 1024)
	, m_primitivesBuffer(2 * 1024 * 1024)
{ }

} // spt::gfx
