#include "DescriptorPoolSet.h"

namespace spt::vulkan
{
void DescriptorPoolSet::AllocateDescriptorSets(const lib::DynamicArray<VkDescriptorSetLayout>& layouts, lib::DynamicArray<VkDescriptorSet>& outDescriptorSets)
{
	SPT_PROFILER_FUNCTION();

	// first try alocate from existing pools
	for (SizeType idx = 0; idx < m_descriptorPools.size(); ++idx)
	{
		const Bool success = m_descriptorPools[idx].AllocateDescriptorSets(layouts, outDescriptorSets);
		if (success)
		{
			return;
		}
	}

	// if none of existing pools could allocate descriptors, create new pool
	DescriptorPool& newPool = m_descriptorPools.emplace_back(DescriptorPool());
	InitializeDescriptorPool(newPool);

	const Bool success = newPool.AllocateDescriptorSets(layouts, outDescriptorSets);
	SPT_CHECK(success);
}

void DescriptorPoolSet::FreeAllDescriptorPools()
{
	std::for_each(std::begin(m_descriptorPools), std::end(m_descriptorPools),
				  [](DescriptorPool& pool)
				  {
					  pool.ResetPool();
				  });

	m_descriptorPools.clear();
}

void DescriptorPoolSet::InitializeDescriptorPool(DescriptorPool& pool)
{
	SPT_PROFILER_FUNCTION();

	static constexpr lib::StaticArray<VkDescriptorPoolSize, 8> poolSizes =
	{
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLER, 256 },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256 },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1024 },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024 },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 512 },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024 },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 512 },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1024 }
	};

	constexpr Uint32 maxSetsNum = std::accumulate(std::cbegin(poolSizes), std::cend(poolSizes), 0,
												  [](Uint32 accumulatedCount, const VkDescriptorPoolSize& poolSize)
												  {
													  return accumulatedCount + poolSize.descriptorCount;
												  });

	constexpr VkDescriptorPoolCreateFlags flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

	pool.Initialize(flags, maxSetsNum, poolSizes.data(), static_cast<Uint32>(poolSizes.size()));
}

} // spt::vulkan
