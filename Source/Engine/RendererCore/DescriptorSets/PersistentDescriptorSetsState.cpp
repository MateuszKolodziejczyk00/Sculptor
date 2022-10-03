#include "PersistentDescriptorSetsState.h"
#include "ResourcesManager.h"
#include "ShaderMetaData.h"
#include "Types/DescriptorSetState.h"
#include "Types/Pipeline/Pipeline.h"

namespace spt::rdr
{

PersistentDescriptorSetsState::PersistentDescriptorSetsState() = default;

void PersistentDescriptorSetsState::UpdatePersistentDescriptors()
{
	SPT_PROFILER_FUNCTION();

	const lib::LockGuard lockGuard(m_createdDescriptorSetsLock);

	FlushCreatedDescriptorSets();
	RemoveInvalidSets();
	UpdateDescriptorSets();
}

rhi::RHIDescriptorSet PersistentDescriptorSetsState::GetOrCreateDescriptorSet(const lib::SharedRef<Pipeline>& pipeline, const lib::SharedRef<DescriptorSetState>& state, Uint32 descriptorSetIdx)
{
	SPT_PROFILER_FUNCTION();

	const auto foundDescriptorSet = m_cachedDescriptorSets.find(state->GetID());

	if (foundDescriptorSet != std::cend(m_cachedDescriptorSets))
	{
		SPT_CHECK(foundDescriptorSet->second.IsValid());
		return foundDescriptorSet->second;
	}

	const lib::LockGuard lockGuard(m_createdDescriptorSetsLock);

	rhi::RHIDescriptorSet& createdDS = m_createdDescriptorSets[state->GetID()];

	if (!createdDS.IsValid())
	{
		const rhi::DescriptorSetLayoutID dsLayoutID = pipeline->GetRHI().GetDescriptorSetLayoutID(descriptorSetIdx);
		createdDS = rhi::RHIDescriptorSetManager::GetInstance().AllocateDescriptorSet(dsLayoutID);

		PersistentDSData newDSData;
		newDSData.id = state->GetID();
		newDSData.metaData = pipeline->GetMetaData();
		newDSData.state = state.ToSharedPtr();
		m_dsData.emplace_back(newDSData);

		DescriptorSetWriter writer = ResourcesManager::CreateDescriptorSetWriter();
		DescriptorSetUpdateContext updateContext(createdDS, writer, pipeline->GetMetaData());
		state->UpdateDescriptors(updateContext);
		writer.Flush();

		// clear flag if that's first time when state is used
		state->ClearDirtyFlag();
	}

	return createdDS;
}

void PersistentDescriptorSetsState::FlushCreatedDescriptorSets()
{
	SPT_PROFILER_FUNCTION();

	std::move(std::begin(m_createdDescriptorSets), std::end(m_createdDescriptorSets), std::inserter(m_cachedDescriptorSets, std::end(m_cachedDescriptorSets)));
	m_createdDescriptorSets.clear();
}

void PersistentDescriptorSetsState::RemoveInvalidSets()
{
	SPT_PROFILER_FUNCTION();

	// remove all sets with expired descriptor set states
	const auto removedBegin = std::remove_if(std::begin(m_dsData), std::end(m_dsData),
											 [](const PersistentDSData& ds)
											 {
												 return ds.state.expired();
											 });

	// destroy descriptor sets
	std::for_each(removedBegin, std::end(m_dsData),
				  [this](const PersistentDSData& dsData)
				  {
					  const rhi::RHIDescriptorSet dsSet = m_cachedDescriptorSets.at(dsData.id);

					  SPT_CHECK_NO_ENTRY(); // destroy set (need to add option for renderer (currently only in rhi)

					  m_cachedDescriptorSets.erase(dsData.id);
				  });

	m_dsData.erase(removedBegin, std::cend(m_dsData));
}

void PersistentDescriptorSetsState::UpdateDescriptorSets()
{
	SPT_PROFILER_FUNCTION();

	lib::DynamicArray<lib::SharedPtr<DescriptorSetState>> dirtyStates;
	dirtyStates.reserve(64);

	DescriptorSetWriter writer = ResourcesManager::CreateDescriptorSetWriter();

	for (const PersistentDSData& ds : m_dsData)
	{
		lib::SharedPtr<DescriptorSetState> state = ds.state.lock();
		SPT_CHECK(!!state);

		if (state->IsDirty())
		{
			DescriptorSetUpdateContext updateContext(m_cachedDescriptorSets.at(ds.id), writer, lib::Ref(ds.metaData));
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
