#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rdr
{

class GraphicsPipeline;
class ComputePipeline;
class DescriptorSetState;
class CommandQueue;


class PipelinePendingState
{
public:

	PipelinePendingState();
	~PipelinePendingState();

	// Graphics Pipeline ================================================
	
	void									BindGraphicsPipeline(const lib::SharedRef<GraphicsPipeline>& pipeline);
	void									UnbindGraphicsPipeline();
	const lib::SharedPtr<GraphicsPipeline>&	GetBoundGraphicsPipeline() const;

	void									FlushDirtyDSForGraphicsPipeline(CommandQueue& cmdQueue);

	// Compute Pipeline =================================================
	
	// Descriptor Set States ============================================
	
	void BindDescriptorSetState(const lib::SharedRef<DescriptorSetState>& state);
	void UnbindDescriptorSetState(const lib::SharedRef<DescriptorSetState>& state);

private:

	void TryMarkAsDirty(const lib::SharedRef<DescriptorSetState>& state);
	lib::SharedPtr<DescriptorSetState> GetBoundDescriptorSetState(SizeType hash) const;

	lib::SharedPtr<GraphicsPipeline> m_boundGraphicsPipeline;

	lib::DynamicArray<lib::SharedPtr<DescriptorSetState>> m_boundDescriptorSetStates;

	lib::DynamicArray<Bool> m_dirtyDescriptorSets;
};

} // spt::rdr
