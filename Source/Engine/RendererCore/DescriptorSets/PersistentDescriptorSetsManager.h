#pragma once

#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIDescriptorSetImpl.h"
#include "Types/DescriptorSetState/DescriptorSetStateTypes.h"


namespace spt::smd
{
class ShaderMetaData;
} // spt::smd


namespace spt::rdr
{

class Pipeline;
class DescriptorSetState;


/**
 * Persistent descriptor sets states are states with 'Persistent' flag
 * These states have global, shared descriptor sets, which are cached between frames
 * Creation and Destruction of such descriptors is slow, as it's currently not batched
 * and during first frame after creation requires locked access (as they are shared, and we don't want to created multiple descriptors in multiple threads)
 */
class PersistentDescriptorSetsManager
{
	using PersistentDSHash = SizeType;

public:

	PersistentDescriptorSetsManager();

	void UpdatePersistentDescriptors();

	rhi::RHIDescriptorSet GetDescriptorSet(const lib::SharedRef<Pipeline>& pipeline, Uint32 descriptorSetIdx, const lib::MTHandle<DescriptorSetState>& state) const;

	rhi::RHIDescriptorSet GetOrCreateDescriptorSet(const lib::SharedRef<Pipeline>& pipeline, Uint32 descriptorSetIdx, const lib::MTHandle<DescriptorSetState>& state);

	void UnregisterDescriptorSet(const DescriptorSetState& state);

private:

	void FlushCreatedDescriptorSets();
	void RemoveInvalidSets();
	void UpdateDescriptorSets();

	PersistentDSHash HashPersistentDS(const lib::SharedRef<Pipeline>& pipeline, Uint32 descriptorSetIdx, const lib::MTHandle<DescriptorSetState>& state) const;

	/**
	 * Descriptors created in current frame.
	 * Access requires synchranization using lock
	 */
	lib::HashMap<PersistentDSHash, rhi::RHIDescriptorSet>	m_createdDescriptorSets;
	mutable lib::Lock										m_createdDescriptorSetsLock;

	/**
	 * Descriptors created in previous frames
	 * Access doesn't require lock
	 * Can be modified only during UpdatePersistentDescriptors
	 */
	lib::HashMap<PersistentDSHash, rhi::RHIDescriptorSet> m_cachedDescriptorSets;

	struct PersistentDSData
	{
		PersistentDSData()
			: hash(idxNone<PersistentDSHash>)
			, lastUpdateFrame(0)
			, descriptorSetIdx(idxNone<Uint32>)
		{ }

		PersistentDSHash          hash;
		lib::WeakPtr<Pipeline>    pipeline;
		const DescriptorSetState* state;
		Uint64                    lastUpdateFrame;
		Uint32 	                  descriptorSetIdx;
	};

	lib::HashSet<const DescriptorSetState*> m_destroyedDescriptors;
	lib::DynamicArray<PersistentDSData>     m_dsData;
};

} // spt::rdr