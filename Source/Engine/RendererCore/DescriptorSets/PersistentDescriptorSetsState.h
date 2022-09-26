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

	void RemoveInvalidSets();
	void UpdateDescriptorSets();

	lib::HashMap<DSStateID, rhi::RHIDescriptorSet> m_descriptorSets;

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

	lib::DynamicArray<PersistentDSData> dsData;
};

} // spt::rdr