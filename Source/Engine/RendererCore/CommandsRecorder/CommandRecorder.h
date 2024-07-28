#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIBridge/RHICommandBufferImpl.h"
#include "Pipelines/PipelineState.h"
#include "PipelinePendingState.h"
#include "RendererUtils.h"
#include "RHIBridge/RHIDependencyImpl.h"


namespace spt::rdr
{

class CommandBuffer;
class RenderingDefinition;
class UIBackend;
class RenderContext;
class DescriptorSetState;
class Texture;
class Buffer;
class BufferView;
class Event;
class TopLevelAS;
class BottomLevelAS;
class QueryPool;
class GPUWorkload;


class RENDERER_CORE_API CommandRecorder
{
public:

	CommandRecorder(const rdr::RendererResourceName& name, const lib::SharedRef<RenderContext>& context, const rhi::CommandBufferDefinition& cmdBufferDef, const rhi::CommandBufferUsageDefinition& commandBufferUsage);

	lib::SharedRef<GPUWorkload> 			FinishRecording();

	const lib::SharedPtr<CommandBuffer>&	GetCommandBuffer() const;

	void									ExecuteBarrier(rhi::RHIDependency& dependency);

	void									SetEvent(const lib::SharedRef<Event>& event, rhi::RHIDependency& dependency);
	void									WaitEvent(const lib::SharedRef<Event>& event, rhi::RHIDependency& dependency);

	void									BuildBLAS(const lib::SharedRef<BottomLevelAS>& blas, const lib::SharedRef<Buffer>& scratchBuffer, Uint64 scratchBufferOffset);
	void									BuildTLAS(const lib::SharedRef<TopLevelAS>& tlas, const lib::SharedRef<Buffer>& scratchBuffer, Uint64 scratchBufferOffset, const lib::SharedRef<Buffer>& instancesBuildDataBuffer);

	void									BeginRendering(const RenderingDefinition& definition);
	void									EndRendering();
	
	void									SetViewport(const math::AlignedBox2f& renderingViewport, Real32 minDepth, Real32 maxDepth);
	void									SetScissor(const math::AlignedBox2u& renderingScissor);

	void									DrawIndirectCount(const lib::SharedRef<Buffer>& drawsBuffer, Uint64 drawsOffset, Uint32 drawsStride, const lib::SharedRef<Buffer>& countBuffer, Uint64 countOffset, Uint32 maxDrawsCount);
	void									DrawIndirectCount(const BufferView& drawsBufferView, Uint64 drawsOffset, Uint32 drawsStride, const BufferView& countBufferView, Uint64 countOffset, Uint32 maxDrawsCount);

	void									DrawIndirect(const lib::SharedRef<Buffer>& drawsBuffer, Uint64 drawsOffset, Uint32 drawsStride, Uint32 drawsCount);
	void									DrawIndirect(const BufferView& drawsBufferView, Uint64 drawsOffset, Uint32 drawsStride, Uint32 drawsCount);
	
	void									DrawInstances(Uint32 verticesNum, Uint32 instancesNum, Uint32 firstVertex = 0, Uint32 firstInstance = 0);

	void									DrawMeshTasks(const math::Vector3u& groupCount);

	void									DrawMeshTasksIndirect(const lib::SharedRef<Buffer>& drawsBuffer, Uint64 drawsOffset, Uint32 drawsStride, Uint32 drawsCount);

	void									DrawMeshTasksIndirectCount(const lib::SharedRef<Buffer>& drawsBuffer, Uint64 drawsOffset, Uint32 drawsStride, const lib::SharedRef<Buffer>& countBuffer, Uint64 countOffset, Uint32 maxDrawsCount);

	void									BindGraphicsPipeline(PipelineStateID pipelineID);
	void									UnbindGraphicsPipeline();

	void									BindComputePipeline(PipelineStateID pipelineID);
	void									BindComputePipeline(const ShaderID& shader);
	void									UnbindComputePipeline();

	void									BindRayTracingPipeline(PipelineStateID pipelineID);
	void									UnbindRayTracingPipeline();

	void									Dispatch(const math::Vector3u& groupCount);

	void									DispatchIndirect(const lib::SharedRef<Buffer>& indirectArgsBuffer, Uint64 indirectArgsOffset);
	void									DispatchIndirect(const BufferView& indirectArgsBufferView, Uint64 indirectArgsOffset);

	void									TraceRays(const math::Vector3u& traceCount);
	void									TraceRaysIndirect(const lib::SharedRef<Buffer>& indirectArgsBuffer, Uint64 indirectArgsOffset);
	void									TraceRaysIndirect(const BufferView& indirectArgsBufferView, Uint64 indirectArgsOffset);

	void									BindDescriptorSetState(const lib::MTHandle<DescriptorSetState>& state);
	void									UnbindDescriptorSetState(const lib::MTHandle<DescriptorSetState>& state);

	template<typename TDescriptorSetStatesRange>
	void									BindDescriptorSetStates(TDescriptorSetStatesRange&& states);

	template<typename TDescriptorSetStatesRange>
	void									UnbindDescriptorSetStates(TDescriptorSetStatesRange&& states);

	void									ExecuteCommands(const lib::SharedRef<rdr::GPUWorkload>& workload);

	void									BlitTexture(const lib::SharedRef<Texture>& source, Uint32 sourceMipLevel, Uint32 sourceArrayLayer, const lib::SharedRef<Texture>& dest, Uint32 destMipLevel, Uint32 destArrayLayer, rhi::ETextureAspect aspect, rhi::ESamplerFilterType filterMode);

	void									ClearTexture(const lib::SharedRef<Texture>& texture, const rhi::ClearColor& clearColor, const rhi::TextureSubresourceRange& subresourceRange);

	void									CopyTexture(const lib::SharedRef<Texture>& source, const rhi::TextureCopyRange& sourceRange, const lib::SharedRef<Texture>& target, const rhi::TextureCopyRange& targetRange, const math::Vector3u& extent);
	void									CopyBuffer(const lib::SharedRef<Buffer>& sourceBuffer, Uint64 sourceOffset, const lib::SharedRef<Buffer>& destBuffer, Uint64 destOffset, Uint64 size);
	void									FillBuffer(const lib::SharedRef<Buffer>& buffer, Uint64 offset, Uint64 range, Uint32 data);
	 
	void									CopyBufferToTexture(const lib::SharedRef<Buffer>& buffer, Uint64 bufferOffset, const lib::SharedRef<Texture>& texture, rhi::ETextureAspect aspect, math::Vector3u copyExtent, math::Vector3u copyOffset = math::Vector3u::Zero(),  Uint32 mipLevel = 0, Uint32 arrayLayer = 0);

	void									InitializeUIFonts();

	void									RenderUI();

	// Debug ============================================

	void	BeginDebugRegion(const lib::HashedString& name, const lib::Color& color);
	void	EndDebugRegion();

#if SPT_ENABLE_GPU_CRASH_DUMPS
	void	SetDebugCheckpoint(const lib::HashedString& marker);
#endif // SPT_ENABLE_GPU_CRASH_DUMPS

	void	ResetQueryPool(const lib::SharedRef<QueryPool>& queryPool, Uint32 firstQueryIdx, Uint32 queryCount);

	void	WriteTimestamp(const lib::SharedRef<QueryPool>& queryPool, Uint32 queryIdx, rhi::EPipelineStage stage);

	void	BeginQuery(const lib::SharedRef<QueryPool>& queryPool, Uint32 queryIdx);
	void	EndQuery(const lib::SharedRef<QueryPool>& queryPool, Uint32 queryIdx);

private:

	rhi::RHICommandBuffer& GetCommandBufferRHI() const;

	lib::SharedPtr<CommandBuffer> m_commandsBuffer;

	Bool m_canReuseCommandBuffer;

	PipelinePendingState m_pipelineState;
};

template<typename TDescriptorSetStatesRange>
void CommandRecorder::BindDescriptorSetStates(TDescriptorSetStatesRange&& states)
{
	for (const auto& state : states)
	{
		BindDescriptorSetState(state);
	}
}

template<typename TDescriptorSetStatesRange>
void CommandRecorder::UnbindDescriptorSetStates(TDescriptorSetStatesRange&& states)
{
	for (const auto& state : states)
	{
		UnbindDescriptorSetState(state);
	}
}

} // spt::rdr
