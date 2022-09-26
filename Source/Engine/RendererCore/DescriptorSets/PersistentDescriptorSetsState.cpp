#include "PersistentDescriptorSetsState.h"
#include "RendererBuilder.h"
#include "ShaderMetaData.h"
#include "Types/DescriptorSetState.h"
#include "Types/Pipeline/Pipeline.h"

namespace spt::rdr
{

PersistentDescriptorSetsState::PersistentDescriptorSetsState() = default;

void PersistentDescriptorSetsState::UpdatePersistentDescriptors()
{
	SPT_PROFILER_FUNCTION();

	RemoveInvalidSets();
	UpdateDescriptorSets();
}

rhi::RHIDescriptorSet PersistentDescriptorSetsState::GetOrCreateDescriptorSet(const lib::SharedRef<Pipeline>& pipeline, const lib::SharedRef<DescriptorSetState>& state, Uint32 descriptorSetIdx)
{
	SPT_PROFILER_FUNCTION();

	rhi::RHIDescriptorSet& foundDescriptorSet = m_descriptorSets[state->GetID()];

	if (foundDescriptorSet.IsValid())
	{
		const rhi::DescriptorSetLayoutID dsLayoutID = pipeline->GetRHI().GetDescriptorSetLayoutID(descriptorSetIdx);
		foundDescriptorSet = rhi::RHIDescriptorSetManager::GetInstance().AllocateDescriptorSet(dsLayoutID);

		PersistentDSData newDSData;
		newDSData.id = state->GetID();
		newDSData.metaData = pipeline->GetMetaData();
		newDSData.state = state.ToSharedPtr();
		dsData.emplace_back(newDSData);
	}

	return foundDescriptorSet;
}

void PersistentDescriptorSetsState::RemoveInvalidSets()
{
	SPT_PROFILER_FUNCTION();

	// remove all sets with expired descriptor set states
	const auto removedBegin = std::remove_if(std::begin(dsData), std::end(dsData),
											 [](const PersistentDSData& ds)
											 {
												 return ds.state.expired();
											 });

	// destroy descriptor sets
	std::for_each(removedBegin, std::end(dsData),
				  [this](const PersistentDSData& dsData)
				  {
					  const rhi::RHIDescriptorSet dsSet = m_descriptorSets.at(dsData.id);

					  SPT_CHECK_NO_ENTRY(); // destroy set (need to add option for renderer (currently only in rhi)

					  m_descriptorSets.erase(dsData.id);
				  });

	dsData.erase(removedBegin, std::cend(dsData));
}

void PersistentDescriptorSetsState::UpdateDescriptorSets()
{
	SPT_PROFILER_FUNCTION();

	lib::DynamicArray<lib::SharedPtr<DescriptorSetState>> dirtyStates;
	dirtyStates.reserve(64);

	DescriptorSetWriter writer = RendererBuilder::CreateDescriptorSetWriter();

	for (const PersistentDSData& ds : dsData)
	{
		lib::SharedPtr<DescriptorSetState> state = ds.state.lock();
		SPT_CHECK(!!state);

		if (state->IsDirty())
		{
			DescriptorSetUpdateContext updateContext(m_descriptorSets.at(ds.id), writer, ds.metaData);
			state->UpdateDescriptors(updateContext);

			dirtyStates.emplace_back(std::move(state));
		}
	}

	writer.Flush();

	for_each(std::begin(dirtyStates), std::end(dirtyStates),
			 [](const lib::SharedPtr<DescriptorSetState>& state)
			 {
				 state->ClearDirtyFlag();
			 });
}

} // spt::rdr
