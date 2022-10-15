#pragma once

#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIDescriptorSetImpl.h"


namespace spt::rdr
{

class GraphicsPipeline;
class ComputePipeline;
class Pipeline;
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

	void									EnqueueFlushDirtyDSForGraphicsPipeline(CommandQueue& cmdQueue);

	// Compute Pipeline =================================================
	
	void									BindComputePipeline(const lib::SharedRef<ComputePipeline>& pipeline);
	void									UnbindComputePipeline();
	const lib::SharedPtr<ComputePipeline>&	GetBoundComputePipeline() const;

	void									EnqueueFlushDirtyDSForComputePipeline(CommandQueue& cmdQueue);
	
	// Descriptor Set States ============================================
	
	void BindDescriptorSetState(const lib::SharedRef<DescriptorSetState>& state);
	void UnbindDescriptorSetState(const lib::SharedRef<DescriptorSetState>& state);
	
	// ===================================================================

	void PostCommandsSubmit();

private:

	using DynamicOffsetsArray = lib::DynamicInlineArray<Uint32, 2>;

	struct DSBindCommandData
	{
		Uint32					idx;
		rhi::RHIDescriptorSet	ds;
		DynamicOffsetsArray		dynamicOffsets;
	};

	struct BoundDescriptorSetState
	{
		lib::SharedPtr<DescriptorSetState>	instance;
		// snapshotted state of dynamic offsets during bound
		DynamicOffsetsArray					dynamicOffsets;
	};

	void TryMarkAsDirty(const lib::SharedRef<DescriptorSetState>& state);
	void TryMarkAsDirtyImpl(const lib::SharedRef<DescriptorSetState>& state, const lib::SharedPtr<Pipeline>& pipeline, lib::DynamicArray<Bool>& dirtyDescriptorSets);

	void UpdateDescriptorSetsOnPipelineChange(const lib::SharedPtr<Pipeline>& prevPipeline, const lib::SharedRef<Pipeline>& newPipeline, lib::DynamicArray<Bool>& dirtyDescriptorSets);

	lib::DynamicArray<DSBindCommandData> FlushPendingDescriptorSets(const lib::SharedRef<Pipeline>& pipeline, lib::DynamicArray<Bool>& dirtyDescriptorSets);

	const BoundDescriptorSetState* GetBoundDescriptorSetState(SizeType hash) const;

	lib::SharedPtr<GraphicsPipeline> m_boundGfxPipeline;
	lib::SharedPtr<ComputePipeline> m_boundComputePipeline;

	lib::DynamicArray<BoundDescriptorSetState> m_boundDescriptorSetStates;

	lib::DynamicArray<Bool> m_dirtyGfxDescriptorSets;
	lib::DynamicArray<Bool> m_dirtyComputeDescriptorSets;
};

} // spt::rdr
