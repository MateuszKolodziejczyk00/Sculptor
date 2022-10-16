#include "DynamicDescriptorSetsManager.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "Types/Pipeline/Pipeline.h"
#include "Types/Context.h"
#include "ResourcesManager.h"

namespace spt::rdr
{

DynamicDescriptorSetsManager::DynamicDescriptorSetsManager()
{ }

void DynamicDescriptorSetsManager::BuildDescriptorSets(Context& renderContext, const lib::DynamicArray<DynamicDescriptorSetInfo>& dsInfos)
{
	SPT_PROFILER_FUNCTION();

	rhi::RHIContext& rhiRenderContext = renderContext.GetRHI();
	SPT_CHECK(rhiRenderContext.IsValid());

	// discard descriptors that are currently created
	lib::DynamicArray<DynamicDescriptorSetInfo> dsInfosToAllocate;
	dsInfosToAllocate.reserve(dsInfos.size());
	std::copy_if(std::cbegin(dsInfos), std::cend(dsInfos), std::back_inserter(dsInfosToAllocate),
				 [&](const DynamicDescriptorSetInfo& dsInfo)
				 {
					 return !m_descriptorSets.contains(dsInfo.state->GetID());
				 });

	// if all descriptors were already created, we don't have to do anything
	if (dsInfosToAllocate.empty())
	{
		return;
	}

	// build layouts array
	lib::DynamicArray<rhi::DescriptorSetLayoutID> dsLayouts;
	dsLayouts.reserve(dsInfosToAllocate.size());
	std::transform(std::cbegin(dsInfosToAllocate), std::cend(dsInfosToAllocate), std::back_inserter(dsLayouts),
				   [](const DynamicDescriptorSetInfo& dsInfo)
				   {
					   SPT_CHECK(dsInfo.pipeline);
					   return dsInfo.pipeline->GetRHI().GetDescriptorSetLayoutID(dsInfo.dsIdx);
				   });

	// allocate descriptor sets
	const lib::DynamicArray<rhi::RHIDescriptorSet> allocatedDescriptorSets = rhiRenderContext.AllocateDescriptorSets(dsLayouts.data(), static_cast<Uint32>(dsLayouts.size()));

	SPT_CHECK(allocatedDescriptorSets.size() == dsInfosToAllocate.size());

	// write to descriptor sets
	DescriptorSetWriter writer = ResourcesManager::CreateDescriptorSetWriter();

	for (SizeType idx = 0; idx < dsInfosToAllocate.size(); ++idx)
	{
		const lib::SharedPtr<Pipeline>& pipeline = dsInfosToAllocate[idx].pipeline;
		const lib::SharedRef<smd::ShaderMetaData>& metaData = pipeline->GetMetaData();

		DescriptorSetUpdateContext updateContext(allocatedDescriptorSets[idx], writer, metaData);
		dsInfosToAllocate[idx].state->UpdateDescriptors(updateContext);
	}

	writer.Flush();

	// store all descriptor sets
	m_descriptorSets.reserve(m_descriptorSets.size() + dsInfosToAllocate.size());

	for (SizeType idx = 0; idx < dsInfosToAllocate.size(); ++idx)
	{
		m_descriptorSets.emplace(dsInfosToAllocate[idx].state->GetID(), allocatedDescriptorSets[idx]);
	}
}

rhi::RHIDescriptorSet DynamicDescriptorSetsManager::GetDescriptorSet(DSStateID id) const
{
	SPT_PROFILER_FUNCTION();

	return m_descriptorSets.at(id);
}

} // spt::rdr
