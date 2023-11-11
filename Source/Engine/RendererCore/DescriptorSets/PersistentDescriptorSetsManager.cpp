#include "PersistentDescriptorSetsManager.h"
#include "ResourcesManager.h"
#include "ShaderMetaData.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "Types/Pipeline/Pipeline.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "Renderer.h"

namespace spt::rdr
{

PersistentDescriptorSetsManager::PersistentDescriptorSetsManager() = default;

void PersistentDescriptorSetsManager::UpdatePersistentDescriptors()
{
	SPT_PROFILER_FUNCTION();

	const lib::LockGuard lockGuard(m_createdDescriptorSetsLock);

	FlushCreatedDescriptorSets();
	RemoveInvalidSets();
	UpdateDescriptorSets();
}

rhi::RHIDescriptorSet PersistentDescriptorSetsManager::GetDescriptorSet(const lib::SharedRef<Pipeline>& pipeline, Uint32 descriptorSetIdx, const lib::MTHandle<DescriptorSetState>& state) const
{
	SPT_PROFILER_FUNCTION();

	const PersistentDSHash dsHash = HashPersistentDS(pipeline, descriptorSetIdx, state);

	rhi::RHIDescriptorSet ds;

	const auto foundDescriptorSet = m_cachedDescriptorSets.find(dsHash);
	if (foundDescriptorSet != std::cend(m_cachedDescriptorSets))
	{
		ds = foundDescriptorSet->second;
	}
	else
	{
		const lib::LockGuard lockGuard(m_createdDescriptorSetsLock);
		ds = m_createdDescriptorSets.at(dsHash);
	}

	SPT_CHECK(ds.IsValid());
	return ds;
}

rhi::RHIDescriptorSet PersistentDescriptorSetsManager::GetOrCreateDescriptorSet(const lib::SharedRef<Pipeline>& pipeline, Uint32 descriptorSetIdx, const lib::MTHandle<DescriptorSetState>& state)
{
	SPT_PROFILER_FUNCTION();

	const PersistentDSHash dsHash = HashPersistentDS(pipeline, descriptorSetIdx, state);

	const auto foundDescriptorSet = m_cachedDescriptorSets.find(dsHash);

	if (foundDescriptorSet != std::cend(m_cachedDescriptorSets))
	{
		SPT_CHECK(foundDescriptorSet->second.IsValid());
		return foundDescriptorSet->second;
	}

	const lib::LockGuard lockGuard(m_createdDescriptorSetsLock);

	rhi::RHIDescriptorSet& createdDS = m_createdDescriptorSets[dsHash];

	if (!createdDS.IsValid())
	{
		SPT_PROFILER_SCOPE("CreateNewPersistentDescriptorSet");

		const rhi::DescriptorSetLayoutID dsLayoutID = pipeline->GetRHI().GetDescriptorSetLayoutID(descriptorSetIdx);
		createdDS = rhi::RHI::AllocateDescriptorSet(dsLayoutID);
		createdDS.SetName(state->GetName());

		PersistentDSData newDSData;
		newDSData.hash              = dsHash;
		newDSData.pipeline          = pipeline.ToSharedPtr();
		newDSData.state             = state.Get();
		newDSData.lastUpdateFrame   = rdr::Renderer::GetCurrentFrameIdx() + 1;
		newDSData.descriptorSetIdx  = descriptorSetIdx;
		m_dsData.emplace_back(newDSData);

		DescriptorSetWriter writer = ResourcesManager::CreateDescriptorSetWriter();
		DescriptorSetUpdateContext updateContext(createdDS, writer, pipeline->GetMetaData());
		state->UpdateDescriptors(updateContext);
		writer.Flush();
	}

	return createdDS;
}

void PersistentDescriptorSetsManager::UnregisterDescriptorSet(const DescriptorSetState& state)
{
	m_destroyedDescriptors.emplace(&state);
}

void PersistentDescriptorSetsManager::FlushCreatedDescriptorSets()
{
	SPT_PROFILER_FUNCTION();

	std::move(std::begin(m_createdDescriptorSets), std::end(m_createdDescriptorSets), std::inserter(m_cachedDescriptorSets, std::end(m_cachedDescriptorSets)));
	m_createdDescriptorSets.clear();
}

void PersistentDescriptorSetsManager::RemoveInvalidSets()
{
	SPT_PROFILER_FUNCTION();

	// remove all sets with expired descriptor set states
	const auto removedBegin = std::remove_if(std::begin(m_dsData), std::end(m_dsData),
											 [this](const PersistentDSData& ds)
											 {
												 return m_destroyedDescriptors.contains(ds.state) || ds.pipeline.expired();
											 });

	// destroy descriptor sets
	std::for_each(removedBegin, std::end(m_dsData),
				  [this](const PersistentDSData& dsData)
				  {
					  rhi::RHI::FreeDescriptorSet(m_cachedDescriptorSets.at(dsData.hash));

					  m_cachedDescriptorSets.erase(dsData.hash);
				  });

	m_dsData.erase(removedBegin, std::cend(m_dsData));

	m_destroyedDescriptors.clear();
}

void PersistentDescriptorSetsManager::UpdateDescriptorSets()
{
	SPT_PROFILER_FUNCTION();

	const Uint64 currentFrameIdx = rdr::Renderer::GetCurrentFrameIdx();

	DescriptorSetWriter writer = ResourcesManager::CreateDescriptorSetWriter();

	for (PersistentDSData& ds : m_dsData)
	{
		if (ds.state->IsDirty() && ds.lastUpdateFrame < currentFrameIdx)
		{
			lib::SharedPtr<Pipeline> pipeline = ds.pipeline.lock();
			rhi::RHIDescriptorSet& rhiDescriptorSet = m_cachedDescriptorSets[ds.hash];
			rdr::CurrentFrameContext::GetCurrentFrameCleanupDelegate().AddLambda([rhiDescriptorSet]()
																				 {
																					 rhi::RHI::FreeDescriptorSet(rhiDescriptorSet);
																				 });
			const rhi::DescriptorSetLayoutID dsLayoutID = pipeline->GetRHI().GetDescriptorSetLayoutID(ds.descriptorSetIdx);
			rhiDescriptorSet = rhi::RHI::AllocateDescriptorSet(dsLayoutID);
			DescriptorSetUpdateContext updateContext(rhiDescriptorSet, writer, pipeline->GetMetaData());
			ds.state->UpdateDescriptors(updateContext);
			
			ds.lastUpdateFrame = currentFrameIdx;
		}
	}

	writer.Flush();
}

PersistentDescriptorSetsManager::PersistentDSHash PersistentDescriptorSetsManager::HashPersistentDS(const lib::SharedRef<Pipeline>& pipeline, Uint32 descriptorSetIdx, const lib::MTHandle<DescriptorSetState>& state) const
{
	const smd::ShaderMetaData& metaData = pipeline->GetMetaData();

	return lib::HashCombine(metaData.GetDescriptorSetHash(descriptorSetIdx), state->GetID());
}

} // spt::rdr
