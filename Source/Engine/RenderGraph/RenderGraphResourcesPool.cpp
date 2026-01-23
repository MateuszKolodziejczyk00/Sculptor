#include "RenderGraphResourcesPool.h"
#include "ResourcesManager.h"
#include "Renderer.h"
#include "Types/DescriptorSetState/DescriptorSetStateTypes.h"
#include "Types/GPUMemoryPool.h"
#include "Types/Texture.h"

namespace spt::rg
{

static constexpr Uint32 allocatorSize = 1u * 1024u * 1024u;

RenderGraphResourcesPool::RenderGraphResourcesPool()
	: m_constantsAllocators{ rdr::ConstantsAllocator(allocatorSize), rdr::ConstantsAllocator(allocatorSize) }
{
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

	const rdr::DeviceQueuesManager& queuesManager = rdr::Renderer::GetDeviceQueuesManager();
	const rdr::GPUTimelineSection currentlyRecordedSection = queuesManager.GetRecordedSection();

	if (m_lastRecordedSection != currentlyRecordedSection)
	{
		m_lastRecordedSection = currentlyRecordedSection;
		m_constantsAllocatorIdx = (m_constantsAllocatorIdx + 1u) % 2u;
		m_constantsAllocators[m_constantsAllocatorIdx].Reset();
	}
}

lib::SharedPtr<rdr::Texture> RenderGraphResourcesPool::AcquireTexture(const RenderGraphDebugName& name, const rhi::TextureDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo)
{
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

	return texture;
}

void RenderGraphResourcesPool::ReleaseTexture(lib::SharedPtr<rdr::Texture> texture)
{
	const rhi::RHIMemoryRequirements memoryRequirements = texture->GetRHI().GetMemoryRequirements();

	MemoryPoolData& poolData = GetMemoryPoolData(texture->GetRHI().GetAllocationInfo());

	SPT_CHECK(poolData.currentMemoryUsage >= memoryRequirements.size);
	poolData.currentMemoryUsage -= memoryRequirements.size;

	if (texture->GetRHI().IsPlacedAllocation())
	{
		texture->ReleasePlacedAllocation();
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
