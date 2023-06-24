#include "DynamicDescriptorSetsManager.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "Types/Pipeline/Pipeline.h"
#include "Types/RenderContext.h"
#include "ResourcesManager.h"

namespace spt::rdr
{

DynamicDescriptorSetsManager::DynamicDescriptorSetsManager()
{ }

DynamicDescriptorSetsManager::~DynamicDescriptorSetsManager()
{
#if RENDERER_VALIDATION
	rdr::CurrentFrameContext::GetCurrentFrameCleanupDelegate().AddLambda([ descriptorSets = std::move(m_descriptorSets) ]() mutable
																		 {
																			 for (auto& [dsStateID, ds] : descriptorSets)
																			 {
																				 ds.ResetName();
																			 }
																		 });
#endif // RENDERER_VALIDATION
}

void DynamicDescriptorSetsManager::BuildDescriptorSets(RenderContext& renderContext, const lib::DynamicArray<DynamicDescriptorSetInfo>& dsInfos)
{
	SPT_PROFILER_FUNCTION();

	rhi::RHIRenderContext& rhiRenderContext = renderContext.GetRHI();
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
	lib::DynamicArray<rhi::RHIDescriptorSet> allocatedDescriptorSets = rhiRenderContext.AllocateDescriptorSets(dsLayouts.data(), static_cast<Uint32>(dsLayouts.size()));

	SPT_CHECK(allocatedDescriptorSets.size() == dsInfosToAllocate.size());

#if RHI_DEBUG

	// give proper names to allocated descriptor sets
	for (SizeType idx = 0; idx < dsInfosToAllocate.size(); ++idx)
	{
		allocatedDescriptorSets[idx].SetName(dsInfosToAllocate[idx].state->GetName());
	}

#endif // RHI_DEBUG

	// write to descriptor sets
	DescriptorSetWriter writer = ResourcesManager::CreateDescriptorSetWriter();

	for (SizeType idx = 0; idx < dsInfosToAllocate.size(); ++idx)
	{
		const lib::SharedPtr<Pipeline>& pipeline = dsInfosToAllocate[idx].pipeline;
		const smd::ShaderMetaData& metaData = pipeline->GetMetaData();

		DescriptorSetUpdateContext updateContext(allocatedDescriptorSets[idx], writer, metaData);
		dsInfosToAllocate[idx].state->UpdateDescriptors(updateContext);
	}

	writer.Flush();

	// store all descriptor sets
	m_descriptorSets.reserve(m_descriptorSets.size() + dsInfosToAllocate.size());

	for (SizeType idx = 0; idx < dsInfosToAllocate.size(); ++idx)
	{
		m_descriptorSets.emplace(dsInfosToAllocate[idx].dsHash, allocatedDescriptorSets[idx]);
	}
}

rhi::RHIDescriptorSet DynamicDescriptorSetsManager::GetDescriptorSet(SizeType hash) const
{
	SPT_PROFILER_FUNCTION();

	return m_descriptorSets.at(hash);
}

} // spt::rdr
