#pragma once

#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIDescriptorSetImpl.h"
#include "Types/DescriptorSetState/DescriptorSetStateTypes.h"
#include "DescriptorSets/DynamicDescriptorSetsTypes.h"

namespace spt { namespace rdr { class RenderContext; } }


namespace spt::rdr
{

class GraphicsPipeline;
class ComputePipeline;
class RayTracingPipeline;
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

	// Ray Tracing Pipeline =============================================
	
	void										BindRayTracingPipeline(const lib::SharedRef<RayTracingPipeline>& pipeline);
	void										UnbindRayTracingPipeline();
	const lib::SharedPtr<RayTracingPipeline>&	GetBoundRayTracingPipeline() const;

	void										EnqueueFlushDirtyDSForRayTracingPipeline(CommandQueue& cmdQueue);
	
	// Descriptor Set States ============================================
	
	void BindDescriptorSetState(const lib::SharedRef<DescriptorSetState>& state);
	void UnbindDescriptorSetState(const lib::SharedRef<DescriptorSetState>& state);

	// ==================================================================

	void PrepareForExecution(const lib::SharedRef<RenderContext>& renderContext);

private:

	using DynamicOffsetsArray = lib::DynamicArray<Uint32>;

	struct PersistentDSBindCommand
	{
		Uint32					idx;
		rhi::RHIDescriptorSet	ds;
		DynamicOffsetsArray		dynamicOffsets;
	};

	struct DynamicDSBindCommand
	{
		Uint32					idx;
		DSStateID				dsHash;
		DynamicOffsetsArray		dynamicOffsets;
	};

	struct DSBindCommands
	{
		lib::DynamicArray<PersistentDSBindCommand>	persistentDSBinds;
		lib::DynamicArray<DynamicDSBindCommand>		dynamicDSBinds;
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

	DSBindCommands FlushPendingDescriptorSets(const lib::SharedRef<Pipeline>& pipeline, lib::DynamicArray<Bool>& dirtyDescriptorSets);

	const BoundDescriptorSetState* GetBoundDescriptorSetState(SizeType dsTypeHash) const;

	lib::SharedPtr<GraphicsPipeline>	m_boundGfxPipeline;
	lib::SharedPtr<ComputePipeline>		m_boundComputePipeline;
	lib::SharedPtr<RayTracingPipeline>	m_boundRayTracingPipeline;

	lib::DynamicArray<BoundDescriptorSetState> m_boundDescriptorSetStates;

	lib::DynamicArray<Bool> m_dirtyGfxDescriptorSets;
	lib::DynamicArray<Bool> m_dirtyComputeDescriptorSets;
	lib::DynamicArray<Bool> m_dirtyRayTracingDescriptorSets;

	// Dynamic descriptor sets that were bound to the pipeline. Later, used to batch and initialize all of them at once
	lib::DynamicArray<DynamicDescriptorSetInfo>		m_dynamicDescriptorSetInfos;
	lib::HashSet<DSStateID>							m_cachedDynamicDescriptorSets;
};

} // spt::rdr
