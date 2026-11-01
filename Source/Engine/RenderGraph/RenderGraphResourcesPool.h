#pragma once

#include "SculptorCoreTypes.h"
#include "RenderGraphTypes.h"
#include "Types/Texture.h"
#include "RenderGraphMacros.h"
#include "DeviceQueues/DeviceQueuesManager.h"
#include "Utils/ConstantsAllocator.h"


namespace spt::rdr
{
class Buffer;
} // spt::rdr


namespace spt::rg
{

struct RGExecutionContext;


class RENDER_GRAPH_API RenderGraphResourcesPool
{
public:

	RenderGraphResourcesPool();

	void Preallocate(const rhi::RHIAllocationInfo& allocationInfo, Uint64 size);

	void Prepare();

	lib::SharedPtr<rdr::Texture> AcquireTexture(const RGExecutionContext& context, const RenderGraphDebugName& name, const rhi::TextureDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo);
	void ReleaseTexture(const RGExecutionContext& context, lib::SharedPtr<rdr::Texture> texture);

	lib::SharedPtr<rdr::Buffer> AcquireBuffer(const RGExecutionContext& context, const RenderGraphDebugName& name, const rhi::BufferDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo, rdr::BufferViewDescriptorsAllocation descriptorsAllocation);
	void ReleaseBuffer(const RGExecutionContext& context, lib::SharedPtr<rdr::Buffer> buffer);

	Uint64 GetCurrentMemoryUsage() const;

	rdr::ConstantsAllocator& GetConstantsAllocator() { return m_constantsAllocators[m_constantsAllocatorIdx]; }

private:

	Bool CanUseTransientResource(const rhi::RHIAllocationInfo& allocationInfo) const;

	lib::SharedPtr<rdr::Texture> AcquireTransientTexture(const RGExecutionContext& context, const RenderGraphDebugName& name, const rhi::TextureDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo);
	void ReleaseTransientTexture(const RGExecutionContext& context, lib::SharedPtr<rdr::Texture> texture);

	lib::SharedPtr<rdr::Buffer> AcquireTransientBuffer(const RGExecutionContext& context, const RenderGraphDebugName& name, const rhi::BufferDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo, rdr::BufferViewDescriptorsAllocation descriptorsAllocation);
	void ReleaseTransientBuffer(const RGExecutionContext& context, lib::SharedPtr<rdr::Buffer> buffer);

	struct MemoryPoolData
	{
		lib::SharedPtr<rdr::GPUMemoryPool> memoryPool;

		Uint64 currentMemoryUsage = 0u;
		Uint64 maxMemoryUsage     = 0u;
	};

	MemoryPoolData& GetMemoryPoolData(const rhi::RHIAllocationInfo& allocationInfo);

	void DestroyResources();

	lib::HashMap<rhi::EMemoryUsage, MemoryPoolData> m_memoryPools;

	rdr::ConstantsAllocator m_constantsAllocators[2];
	Uint32 m_constantsAllocatorIdx = 0u;

	rdr::GPUTimelineSection m_lastRecordedSection = {};
};

} // spt::rg
