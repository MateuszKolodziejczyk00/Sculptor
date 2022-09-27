#pragma once

#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIDescriptorSetImpl.h"
#include "Types/DescriptorSetState.h"


namespace spt::smd
{
class ShaderMetaData;
}


namespace spt::rdr
{

class Pipeline;


class PersistentDescriptorSetsState
{
public:

	PersistentDescriptorSetsState();

	void UpdatePersistentDescriptors();

	rhi::RHIDescriptorSet GetOrCreateDescriptorSet(const lib::SharedRef<Pipeline>& pipeline, const lib::SharedRef<DescriptorSetState>& state, Uint32 descriptorSetIdx);

private:

	void FlushCreatedDescriptorSets();
	void RemoveInvalidSets();
	void UpdateDescriptorSets();

	/**
	 * Descriptors created in current frame.
	 * Access requires synchranization using lock
	 */
	lib::HashMap<DSStateID, rhi::RHIDescriptorSet>	m_createdDescriptorSets;
	lib::Lock										m_createdDescriptorSetsLock;

	/**
	 * Descriptors created in previous frames
	 * Access doesn't require lock
	 * Can be modified only during UpdatePersistentDescriptors
	 */
	lib::HashMap<DSStateID, rhi::RHIDescriptorSet> m_cachedDescriptorSets;

	struct PersistentDSData
	{
		PersistentDSData()
			: id(idxNone<DSStateID>)
		{ }

		// cached id from state (we need to cache it because otherwise after state is removed we wouldn't know which descriptor should be deleted
		DSStateID							id;
		lib::SharedPtr<smd::ShaderMetaData>	metaData;
		lib::WeakPtr<DescriptorSetState>	state;
	};

	lib::DynamicArray<PersistentDSData> m_dsData;
};

} // spt::rdr