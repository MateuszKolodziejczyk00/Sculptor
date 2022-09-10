#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"
#include "RHIDescriptorSet.h"
#include "Vulkan/DescriptorSets/DescriptorPoolSet.h"
#include "RHICore/RHIDescriptorTypes.h"


namespace spt::vulkan
{

class RHI_API RHIDescriptorSetManager
{
public:

	static void InitializeRHI();
	static void ReleaseRHI();

	SPT_NODISCARD static lib::DynamicArray<RHIDescriptorSet>	AllocateDescriptorSets(const rhi::DescriptorSetLayoutID* LayoutIDs, Uint32 descriptorSetsNum);
	static void													FreeDescriptorSets(const lib::DynamicArray<RHIDescriptorSet>& sets);

private:

	struct DescriptorPoolSetData
	{
		DescriptorPoolSetData() = default;

		DescriptorPoolSet	poolSet;
		lib::Lock			lock;
	};
	
	RHIDescriptorSetManager();
	static RHIDescriptorSetManager& GetInstance();

	void InitializeRHIImpl();
	void ReleaseRHIImpl();

	SPT_NODISCARD lib::DynamicArray<RHIDescriptorSet>	AllocateDescriptorSetsImpl(const rhi::DescriptorSetLayoutID* LayoutIDs, Uint32 descriptorSetsNum);
	void												FreeDescriptorSetsImpl(const lib::DynamicArray<RHIDescriptorSet>& sets);

	SPT_NODISCARD DescriptorPoolSetData&	LockDescriptorPoolSet();
	void									UnlockDescriptorPoolSet(DescriptorPoolSetData& poolSetData);

	static constexpr SizeType poolSetsNum = 4;

	lib::StaticArray<DescriptorPoolSetData, poolSetsNum>	m_poolSets;
};

} // spt::vulkan
