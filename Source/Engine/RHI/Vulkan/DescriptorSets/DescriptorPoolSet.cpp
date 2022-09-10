#include "DescriptorPoolSet.h"
#include "Vulkan/VulkanRHI.h"

namespace spt::vulkan
{

DescriptorPoolSet::DescriptorPoolSet()
	: m_thisSetIdx(idxNone<Uint16>)
{ }

void DescriptorPoolSet::SetPoolSetIdx(Uint16 inSetIdx)
{
	SPT_CHECK(inSetIdx != idxNone<Uint16>);
	m_thisSetIdx = inSetIdx;
}

void DescriptorPoolSet::AllocateDescriptorSets(const VkDescriptorSetLayout* layouts, Uint32 layoutsNum, lib::DynamicArray<RHIDescriptorSet>& outDescriptorSets)
{
	SPT_PROFILER_FUNCTION();

	const auto initDescriptorSets = [this](const lib::DynamicArray<VkDescriptorSet>& handles, Uint16 poolIdx, lib::DynamicArray<RHIDescriptorSet>& outSets)
	{
		outSets.reserve(outSets.size() + handles.size());

		std::transform(std::cbegin(handles), std::cend(handles), std::back_inserter(outSets),
					   [this, poolIdx](VkDescriptorSet ds)
					   {
						   return RHIDescriptorSet(ds, m_thisSetIdx, poolIdx);
					   });
	};

	lib::DynamicArray<VkDescriptorSet> descriptorSetHandles;
	descriptorSetHandles.resize(static_cast<SizeType>(layoutsNum));

	// first try alocate from existing pools
	for (SizeType idx = 0; idx < m_descriptorPools.size(); ++idx)
	{
		const Bool success = m_descriptorPools[idx].AllocateDescriptorSets(layouts, layoutsNum, descriptorSetHandles);
		if (success)
		{
			initDescriptorSets(descriptorSetHandles, static_cast<Uint16>(idx), outDescriptorSets);
			return;
		}
	}

	// if none of existing pools could allocate descriptors, create new pool
	DescriptorPool& newPool = m_descriptorPools.emplace_back(DescriptorPool());
	InitializeDescriptorPool(newPool);

	const Bool success = newPool.AllocateDescriptorSets(layouts, layoutsNum, descriptorSetHandles);
	SPT_CHECK(success);
	initDescriptorSets(descriptorSetHandles, static_cast<Uint16>(m_descriptorPools.size() - 1), outDescriptorSets);
}

void DescriptorPoolSet::FreeDescriptorSets(const lib::DynamicArray<VkDescriptorSet>& dsSets, Uint16 poolIdx)
{
	SPT_PROFILER_FUNCTION();

	const SizeType poolIdxST = static_cast<SizeType>(poolIdx);
	SPT_CHECK(m_descriptorPools.size() < poolIdxST);

	m_descriptorPools[poolIdxST].FreeDescriptorSets(dsSets);
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
