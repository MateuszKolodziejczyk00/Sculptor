#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetStateTypes.h"


namespace spt::rdr
{

class GraphicsPipeline;
class ComputePipeline;
class RayTracingPipeline;
class Pipeline;
class DescriptorSetState;
class CommandQueue;
class RenderContext;


class PipelinePendingState
{
public:

	PipelinePendingState();
	~PipelinePendingState();

	// Graphics Pipeline ================================================
	
	void									BindGraphicsPipeline(const lib::SharedRef<GraphicsPipeline>& pipeline);
	void									UnbindGraphicsPipeline();
	const lib::SharedPtr<GraphicsPipeline>&	GetBoundGraphicsPipeline() const;

	void									FlushDirtyDSForGraphicsPipeline(rhi::RHICommandBuffer& cmdBuffer);

	// Compute Pipeline =================================================
	
	void									BindComputePipeline(const lib::SharedRef<ComputePipeline>& pipeline);
	void									UnbindComputePipeline();
	const lib::SharedPtr<ComputePipeline>&	GetBoundComputePipeline() const;

	void									FlushDirtyDSForComputePipeline(rhi::RHICommandBuffer& cmdBuffer);

	// Ray Tracing Pipeline =============================================
	
	void										BindRayTracingPipeline(const lib::SharedRef<RayTracingPipeline>& pipeline);
	void										UnbindRayTracingPipeline();
	const lib::SharedPtr<RayTracingPipeline>&	GetBoundRayTracingPipeline() const;

	void										FlushDirtyDSForRayTracingPipeline(rhi::RHICommandBuffer& cmdBuffer);
	
	// Descriptor Set States ============================================
	
	void BindDescriptorSetState(const lib::MTHandle<DescriptorSetState>& state);
	void UnbindDescriptorSetState(const lib::MTHandle<DescriptorSetState>& state);

private:

	using DynamicOffsetsArray = lib::DynamicArray<Uint32>;

	struct DSBindCommand
	{
		Uint32 idx;
		Uint32 heapOffset;
	};

	using DSBindCommands = lib::DynamicArray<DSBindCommand>;

	struct BoundDescriptorSetState
	{
		DSStateTypeID typeID;
		Uint32        heapOffset;
	};

	struct PipelineDescriptorsState
	{
		lib::DynamicArray<Bool> dirtyDescriptorSets;
		Bool isBindlessBound = false;
	};

	void TryMarkAsDirty(const lib::MTHandle<DescriptorSetState>& state);
	void TryMarkAsDirtyImpl(const lib::MTHandle<DescriptorSetState>& state, const lib::SharedPtr<Pipeline>& pipeline, PipelineDescriptorsState& descriptorsState);

	void UpdateDescriptorSetsOnPipelineChange(const lib::SharedPtr<Pipeline>& prevPipeline, const lib::SharedRef<Pipeline>& newPipeline, PipelineDescriptorsState& descriptorsState);

	DSBindCommands FlushPendingDescriptorSets(const lib::SharedRef<Pipeline>& pipeline, PipelineDescriptorsState& descriptorsState);

	const BoundDescriptorSetState* GetBoundDescriptorSetState(DSStateTypeID dsTypeID) const;

	lib::SharedPtr<GraphicsPipeline>	m_boundGfxPipeline;
	lib::SharedPtr<ComputePipeline>		m_boundComputePipeline;
	lib::SharedPtr<RayTracingPipeline>	m_boundRayTracingPipeline;

	lib::DynamicArray<BoundDescriptorSetState> m_boundDescriptorSetStates;

	//lib::DynamicArray<Bool> m_dirtyGfxDescriptorSets;
	//lib::DynamicArray<Bool> m_dirtyComputeDescriptorSets;
	//lib::DynamicArray<Bool> m_dirtyRayTracingDescriptorSets;

	PipelineDescriptorsState m_gfxPipelineDescriptorsState;
	PipelineDescriptorsState m_computePipelineDescriptorsState;
	PipelineDescriptorsState m_rayTracingPipelineDescriptorsState;

	Uint32 m_bindlessDescriptorsHeapOffset = 0u;
};

} // spt::rdr
