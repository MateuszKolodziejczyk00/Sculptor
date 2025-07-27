#pragma once

#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIDescriptorSetImpl.h"
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
		Uint32                idx;
#if SPT_USE_DESCRIPTOR_BUFFERS
		Uint32                heapOffset;
#else
		rhi::RHIDescriptorSet ds;
		DynamicOffsetsArray   dynamicOffsets;
#endif // SPT_USE_DESCRIPTOR_BUFFERS
	};

	using DSBindCommands = lib::DynamicArray<DSBindCommand>;

	struct BoundDescriptorSetState
	{
		DSStateTypeID         typeID;
#if SPT_USE_DESCRIPTOR_BUFFERS
		Uint32                heapOffset;
#else
		rhi::RHIDescriptorSet ds;
		// snapshotted state of dynamic offsets during bound
		DynamicOffsetsArray   dynamicOffsets;
#endif // SPT_USE_DESCRIPTOR_BUFFERS
	};

	void TryMarkAsDirty(const lib::MTHandle<DescriptorSetState>& state);
	void TryMarkAsDirtyImpl(const lib::MTHandle<DescriptorSetState>& state, const lib::SharedPtr<Pipeline>& pipeline, lib::DynamicArray<Bool>& dirtyDescriptorSets);

	void UpdateDescriptorSetsOnPipelineChange(const lib::SharedPtr<Pipeline>& prevPipeline, const lib::SharedRef<Pipeline>& newPipeline, lib::DynamicArray<Bool>& dirtyDescriptorSets);

	DSBindCommands FlushPendingDescriptorSets(const lib::SharedRef<Pipeline>& pipeline, lib::DynamicArray<Bool>& dirtyDescriptorSets);

	const BoundDescriptorSetState* GetBoundDescriptorSetState(DSStateTypeID dsTypeID) const;

	lib::SharedPtr<GraphicsPipeline>	m_boundGfxPipeline;
	lib::SharedPtr<ComputePipeline>		m_boundComputePipeline;
	lib::SharedPtr<RayTracingPipeline>	m_boundRayTracingPipeline;

	lib::DynamicArray<BoundDescriptorSetState> m_boundDescriptorSetStates;

	lib::DynamicArray<Bool> m_dirtyGfxDescriptorSets;
	lib::DynamicArray<Bool> m_dirtyComputeDescriptorSets;
	lib::DynamicArray<Bool> m_dirtyRayTracingDescriptorSets;
};

} // spt::rdr
