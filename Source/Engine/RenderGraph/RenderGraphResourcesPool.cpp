#include "RenderGraphResourcesPool.h"
#include "GPUDiagnose/Profiler/GPUStatisticsCollector.h"
#include "ResourcesManager.h"
#include "GPUApi.h"
#include "Types/GPUMemoryPool.h"
#include "Types/Texture.h"

namespace spt::rg
{

static constexpr Uint32 allocatorSize = 1u * 1024u * 1024u;

RenderGraphResourcesPool::RenderGraphResourcesPool()
	: m_constantsAllocators{ rdr::ConstantsAllocator(allocatorSize), rdr::ConstantsAllocator(allocatorSize) }
{
}

void RenderGraphResourcesPool::Preallocate(const rhi::RHIAllocationInfo& allocationInfo, Uint64 size)
{
	MemoryPoolData& poolData = GetMemoryPoolData(allocationInfo);

	poolData.maxMemoryUsage = std::max(poolData.maxMemoryUsage, size);
}

void RenderGraphResourcesPool::Prepare()
{
	SPT_PROFILER_FUNCTION();

	for (auto& [memoryUsage, poolData] : m_memoryPools)
	{
		SPT_CHECK(poolData.currentMemoryUsage == 0u);

		const Uint64 poolSize       = poolData.memoryPool ? poolData.memoryPool->GetRHI().GetSize() : 0u;
		const Uint64 requiredMemory = static_cast<Uint64>(poolData.maxMemoryUsage * 1.2);
		if (poolSize < requiredMemory)
		{
			poolData.memoryPool = rdr::ResourcesManager::CreateGPUMemoryPool(RENDERER_RESOURCE_NAME("Render Graph GPU Memory Pool"), rhi::RHIMemoryPoolDefinition(requiredMemory), rhi::EMemoryUsage::GPUOnly);
		}
	}

	const rdr::DeviceQueuesManager& queuesManager = rdr::GPUApi::GetDeviceQueuesManager();
	const rdr::GPUTimelineSection currentlyRecordedSection = queuesManager.GetRecordedSection();

	if (m_lastRecordedSection != currentlyRecordedSection)
	{
		m_lastRecordedSection = currentlyRecordedSection;
		m_constantsAllocatorIdx = (m_constantsAllocatorIdx + 1u) % 2u;
		m_constantsAllocators[m_constantsAllocatorIdx].Reset();
	}
}

lib::SharedPtr<rdr::Texture> RenderGraphResourcesPool::AcquireTexture(const RGExecutionContext& context, const RenderGraphDebugName& name, const rhi::TextureDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo)
{
	if (CanUseTransientResource(allocationInfo))
	{
		return AcquireTransientTexture(context, name, definition, allocationInfo);
	}
	else
	{
		return rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME(name.Get()), definition, rdr::AllocationDefinition(rdr::CommittedAllocationDef(allocationInfo)));
	}
}

void RenderGraphResourcesPool::ReleaseTexture(const RGExecutionContext& context, lib::SharedPtr<rdr::Texture> texture)
{
	if (CanUseTransientResource(texture->GetRHI().GetAllocationInfo()))
	{
		ReleaseTransientTexture(context, texture);
	}
}

lib::SharedPtr<rdr::Buffer> RenderGraphResourcesPool::AcquireBuffer(const RGExecutionContext& context, const RenderGraphDebugName& name, const rhi::BufferDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo, rdr::BufferViewDescriptorsAllocation descriptorsAllocation)
{
	if (CanUseTransientResource(allocationInfo))
	{
		return AcquireTransientBuffer(context, name, definition, allocationInfo, std::move(descriptorsAllocation));
	}
	else
	{
		return rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME(name.Get()), definition, rdr::AllocationDefinition(rdr::CommittedAllocationDef(allocationInfo)), std::move(descriptorsAllocation));
	}
}

void RenderGraphResourcesPool::ReleaseBuffer(const RGExecutionContext& context, lib::SharedPtr<rdr::Buffer> buffer)
{
	if (CanUseTransientResource(buffer->GetRHI().GetAllocationInfo()))
	{
		ReleaseTransientBuffer(context, buffer);
	}
}

Uint64 RenderGraphResourcesPool::GetCurrentMemoryUsage() const
{
	Uint64 totalMemoryUsage = 0u;
	for (const auto& [memoryUsage, poolData] : m_memoryPools)
	{
		totalMemoryUsage += poolData.currentMemoryUsage;
	}
	return totalMemoryUsage;
}

Bool RenderGraphResourcesPool::CanUseTransientResource(const rhi::RHIAllocationInfo& allocationInfo) const
{
	return allocationInfo.memoryUsage == rhi::EMemoryUsage::GPUOnly;
}

lib::SharedPtr<rdr::Texture> RenderGraphResourcesPool::AcquireTransientTexture(const RGExecutionContext& context, const RenderGraphDebugName& name, const rhi::TextureDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo)
{
	SPT_PROFILER_FUNCTION();

	MemoryPoolData& poolData = GetMemoryPoolData(allocationInfo);

	rdr::AllocationDefinition allocationDefinition;
	if (poolData.memoryPool)
	{
		allocationDefinition.SetAllocationDef(rdr::PlacedAllocationDef(lib::Ref(poolData.memoryPool)));
	}

	const lib::SharedRef<rdr::Texture> texture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME(name.Get()), definition, allocationDefinition);

	rhi::RHIMemoryRequirements memoryRequirements = texture->GetRHI().GetMemoryRequirements();
	poolData.currentMemoryUsage += memoryRequirements.size;
	poolData.maxMemoryUsage      = std::max(poolData.maxMemoryUsage, poolData.currentMemoryUsage);

	if (!texture->HasBoundMemory())
	{
		// If placed failed, do committed allocation
		texture->BindMemory(allocationInfo);
	}

	SPT_CHECK(texture->HasBoundMemory());

#if DEBUG_RENDER_GRAPH
	if (context.statisticsCollector)
	{
		rdr::TransientAllocEventInfo allocationInfoDebug;
		allocationInfoDebug.allocationId = reinterpret_cast<Uint64>(texture.Get());
		allocationInfoDebug.name         = name.Get();
		allocationInfoDebug.size         = memoryRequirements.size;
		context.statisticsCollector->AddScopeTransientAllocation(allocationInfoDebug);
	}
#endif // DEBUG_RENDER_GRAPH

	return texture;
}

void RenderGraphResourcesPool::ReleaseTransientTexture(const RGExecutionContext& context, lib::SharedPtr<rdr::Texture> texture)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RHIMemoryRequirements memoryRequirements = texture->GetRHI().GetMemoryRequirements();

#if DEBUG_RENDER_GRAPH
	if (context.statisticsCollector)
	{
		rdr::TransientAllocEventInfo deallocationInfoDebug;
		deallocationInfoDebug.allocationId = reinterpret_cast<Uint64>(texture.get());
		deallocationInfoDebug.name         = texture->GetRHI().GetName();
		deallocationInfoDebug.size         = memoryRequirements.size;
		context.statisticsCollector->AddScopeTransientDeallocation(deallocationInfoDebug);
	}
#endif // DEBUG_RENDER_GRAPH

	MemoryPoolData& poolData = GetMemoryPoolData(texture->GetRHI().GetAllocationInfo());

	SPT_CHECK(poolData.currentMemoryUsage >= memoryRequirements.size);
	poolData.currentMemoryUsage -= memoryRequirements.size;

	if (texture->GetRHI().IsPlacedAllocation())
	{
		texture->ReleasePlacedAllocation();
	}
}

lib::SharedPtr<rdr::Buffer> RenderGraphResourcesPool::AcquireTransientBuffer(const RGExecutionContext& context, const RenderGraphDebugName& name, const rhi::BufferDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo, rdr::BufferViewDescriptorsAllocation descriptorsAllocation)
{
	SPT_PROFILER_FUNCTION();

	MemoryPoolData& poolData = GetMemoryPoolData(allocationInfo);

	rdr::AllocationDefinition allocationDefinition;
	if (poolData.memoryPool)
	{
		allocationDefinition.SetAllocationDef(rdr::PlacedAllocationDef(lib::Ref(poolData.memoryPool)));
	}

	const lib::SharedRef<rdr::Buffer> buffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME(name.Get()), definition, allocationDefinition, std::move(descriptorsAllocation));

	rhi::RHIMemoryRequirements memoryRequirements = buffer->GetRHI().GetMemoryRequirements();
	poolData.currentMemoryUsage += memoryRequirements.size;
	poolData.maxMemoryUsage      = std::max(poolData.maxMemoryUsage, poolData.currentMemoryUsage);

	if (!buffer->HasBoundMemory())
	{
		// If placed failed, do committed allocation
		buffer->BindMemory(allocationInfo);
	}

	SPT_CHECK(buffer->HasBoundMemory());

	buffer->UpdateDescriptorsMemory();

#if DEBUG_RENDER_GRAPH
	if (context.statisticsCollector)
	{
		rdr::TransientAllocEventInfo allocationInfoDebug;
		allocationInfoDebug.allocationId = reinterpret_cast<Uint64>(buffer.Get());
		allocationInfoDebug.name         = name.Get();
		allocationInfoDebug.size         = memoryRequirements.size;
		context.statisticsCollector->AddScopeTransientAllocation(allocationInfoDebug);
	}
#endif // DEBUG_RENDER_GRAPH

	return buffer;
}

void RenderGraphResourcesPool::ReleaseTransientBuffer(const RGExecutionContext& context, lib::SharedPtr<rdr::Buffer> buffer)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RHIMemoryRequirements memoryRequirements = buffer->GetRHI().GetMemoryRequirements();

#if DEBUG_RENDER_GRAPH
	if (context.statisticsCollector)
	{
		rdr::TransientAllocEventInfo deallocationInfoDebug;
		deallocationInfoDebug.allocationId = reinterpret_cast<Uint64>(buffer.get());
		deallocationInfoDebug.name         = buffer->GetRHI().GetName();
		deallocationInfoDebug.size         = memoryRequirements.size;
		context.statisticsCollector->AddScopeTransientDeallocation(deallocationInfoDebug);
	}
#endif // DEBUG_RENDER_GRAPH

	MemoryPoolData& poolData = GetMemoryPoolData(buffer->GetRHI().GetAllocationInfo());

	SPT_CHECK(poolData.currentMemoryUsage >= memoryRequirements.size);
	poolData.currentMemoryUsage -= memoryRequirements.size;

	if (buffer->GetRHI().IsPlacedAllocation())
	{
		buffer->ReleasePlacedAllocation();
	}
}

RenderGraphResourcesPool::MemoryPoolData& RenderGraphResourcesPool::GetMemoryPoolData(const rhi::RHIAllocationInfo& allocationInfo)
{
	MemoryPoolData& poolData = m_memoryPools[allocationInfo.memoryUsage];
	return poolData;
}

void RenderGraphResourcesPool::DestroyResources()
{
	m_memoryPools.clear();
}

} // spt::rg
