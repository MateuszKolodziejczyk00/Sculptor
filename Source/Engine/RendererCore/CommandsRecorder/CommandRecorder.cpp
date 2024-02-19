#include "CommandRecorder.h"
#include "RenderingDefinition.h"
#include "ResourcesManager.h"
#include "Renderer.h"
#include "Pipelines/PipelinesLibrary.h"
#include "Types/CommandBuffer.h"
#include "Types/UIBackend.h"
#include "Types/Pipeline/GraphicsPipeline.h"
#include "Types/Pipeline/ComputePipeline.h"
#include "Types/Event.h"
#include "Types/Buffer.h"
#include "Types/AccelerationStructure.h"
#include "Types/QueryPool.h"
#include "DeviceQueues/GPUWorkload.h"

namespace spt::rdr
{

CommandRecorder::CommandRecorder(const rdr::RendererResourceName& name, const lib::SharedRef<RenderContext>& context, const rhi::CommandBufferDefinition& cmdBufferDef, const rhi::CommandBufferUsageDefinition& commandBufferUsage)
{
	SPT_PROFILER_FUNCTION();

	m_commandsBuffer = ResourcesManager::CreateCommandBuffer(name, context, cmdBufferDef);
	m_commandsBuffer->StartRecording(commandBufferUsage);

	m_canReuseCommandBuffer = !lib::HasAnyFlag(commandBufferUsage.beginFlags, rhi::ECommandBufferBeginFlags::OneTimeSubmit);
}

lib::SharedRef<GPUWorkload> CommandRecorder::FinishRecording()
{
	SPT_PROFILER_FUNCTION();

	m_commandsBuffer->FinishRecording();

	GPUWorkloadInfo m_pendingWorkloadInfo = GPUWorkloadInfo();
	m_pendingWorkloadInfo.canBeReused = m_canReuseCommandBuffer;
	return lib::MakeShared<GPUWorkload>(lib::Ref(m_commandsBuffer), m_pendingWorkloadInfo);
}

const lib::SharedPtr<CommandBuffer>& CommandRecorder::GetCommandBuffer() const
{
	return m_commandsBuffer;
}

void CommandRecorder::ExecuteBarrier(rhi::RHIDependency& dependency)
{
	SPT_PROFILER_FUNCTION();

	dependency.ExecuteBarrier(GetCommandBufferRHI());
}

void CommandRecorder::SetEvent(const lib::SharedRef<Event>& event, rhi::RHIDependency& dependency)
{
	SPT_PROFILER_FUNCTION();

	dependency.SetEvent(GetCommandBufferRHI(), event->GetRHI());
}

void CommandRecorder::WaitEvent(const lib::SharedRef<Event>& event, rhi::RHIDependency& dependency)
{
	SPT_PROFILER_FUNCTION();

	dependency.WaitEvent(GetCommandBufferRHI(), event->GetRHI());
}

void CommandRecorder::BuildBLAS(const lib::SharedRef<BottomLevelAS>& blas, const lib::SharedRef<Buffer>& scratchBuffer, Uint64 scratchBufferOffset)
{
	SPT_PROFILER_FUNCTION();

	GetCommandBufferRHI().BuildBLAS(blas->GetRHI(), scratchBuffer->GetRHI(), scratchBufferOffset);
}

void CommandRecorder::BuildTLAS(const lib::SharedRef<TopLevelAS>& tlas, const lib::SharedRef<Buffer>& scratchBuffer, Uint64 scratchBufferOffset, const lib::SharedRef<Buffer>& instancesBuildDataBuffer)
{
	SPT_PROFILER_FUNCTION();

	GetCommandBufferRHI().BuildTLAS(tlas->GetRHI(), scratchBuffer->GetRHI(), scratchBufferOffset, instancesBuildDataBuffer->GetRHI());
}

void CommandRecorder::BeginRendering(const RenderingDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	 GetCommandBufferRHI().BeginRendering(definition.GetRHI());
}

void CommandRecorder::EndRendering()
{
	SPT_PROFILER_FUNCTION();

	 GetCommandBufferRHI().EndRendering();
}

void CommandRecorder::SetViewport(const math::AlignedBox2f& renderingViewport, Real32 minDepth, Real32 maxDepth)
{
	SPT_PROFILER_FUNCTION();

	 GetCommandBufferRHI().SetViewport(renderingViewport, minDepth, maxDepth);
}

void CommandRecorder::SetScissor(const math::AlignedBox2u& renderingScissor)
{
	SPT_PROFILER_FUNCTION();

	 GetCommandBufferRHI().SetScissor(renderingScissor);
}

void CommandRecorder::DrawIndirectCount(const lib::SharedRef<Buffer>& drawsBuffer, Uint64 drawsOffset, Uint32 drawsStride, const lib::SharedRef<Buffer>& countBuffer, Uint64 countOffset, Uint32 maxDrawsCount)
{
	SPT_PROFILER_FUNCTION();

	m_pipelineState.FlushDirtyDSForGraphicsPipeline(GetCommandBufferRHI());

	GetCommandBufferRHI().DrawIndirectCount(drawsBuffer->GetRHI(), drawsOffset, drawsStride, countBuffer->GetRHI(), countOffset, maxDrawsCount);
}

void CommandRecorder::DrawIndirectCount(const BufferView& drawsBufferView, Uint64 drawsOffset, Uint32 drawsStride, const BufferView& countBufferView, Uint64 countOffset, Uint32 maxDrawsCount)
{
	SPT_PROFILER_FUNCTION();

	DrawIndirectCount(drawsBufferView.GetBuffer(), drawsBufferView.GetOffset() + drawsOffset, drawsStride, countBufferView.GetBuffer(), countBufferView.GetOffset() + countOffset, maxDrawsCount);
}

void CommandRecorder::DrawIndirect(const lib::SharedRef<Buffer>& drawsBuffer, Uint64 drawsOffset, Uint32 drawsStride, Uint32 drawsCount)
{
	SPT_PROFILER_FUNCTION();

	m_pipelineState.FlushDirtyDSForGraphicsPipeline(GetCommandBufferRHI());

	GetCommandBufferRHI().DrawIndirect(drawsBuffer->GetRHI(), drawsOffset, drawsStride, drawsCount);
}

void CommandRecorder::DrawIndirect(const BufferView& drawsBufferView, Uint64 drawsOffset, Uint32 drawsStride, Uint32 drawsCount)
{
	SPT_PROFILER_FUNCTION();

	DrawIndirect(drawsBufferView.GetBuffer(), drawsBufferView.GetOffset() + drawsOffset, drawsStride, drawsCount);
}

void CommandRecorder::DrawInstances(Uint32 verticesNum, Uint32 instancesNum, Uint32 firstVertex /*= 0*/, Uint32 firstInstance /*= 0*/)
{
	SPT_PROFILER_FUNCTION();

	m_pipelineState.FlushDirtyDSForGraphicsPipeline(GetCommandBufferRHI());

	GetCommandBufferRHI().DrawInstances(verticesNum, instancesNum, firstVertex, firstInstance);
}

void CommandRecorder::BindGraphicsPipeline(PipelineStateID pipelineID)
{
	SPT_PROFILER_FUNCTION();

	const lib::SharedPtr<GraphicsPipeline> pipeline = Renderer::GetPipelinesLibrary().GetGraphicsPipeline(pipelineID);
	SPT_CHECK(!!pipeline);

	m_pipelineState.BindGraphicsPipeline(lib::Ref(pipeline));

	GetCommandBufferRHI().BindGfxPipeline(pipeline->GetRHI());
}

void CommandRecorder::BindComputePipeline(PipelineStateID pipelineID)
{
	SPT_PROFILER_FUNCTION();

	const lib::SharedPtr<ComputePipeline> pipeline = Renderer::GetPipelinesLibrary().GetComputePipeline(pipelineID);
	SPT_CHECK(!!pipeline);

	m_pipelineState.BindComputePipeline(lib::Ref(pipeline));

	GetCommandBufferRHI().BindComputePipeline(pipeline->GetRHI());
}

void CommandRecorder::BindComputePipeline(const ShaderID& shader)
{
	SPT_PROFILER_FUNCTION();

	const PipelineStateID pipelineID = Renderer::GetPipelinesLibrary().GetOrCreateComputePipeline(RENDERER_RESOURCE_NAME(shader.GetName()), shader);
	BindComputePipeline(pipelineID);
}

void CommandRecorder::BindRayTracingPipeline(PipelineStateID pipelineID)
{
	SPT_PROFILER_FUNCTION();

	const lib::SharedPtr<RayTracingPipeline> pipeline = Renderer::GetPipelinesLibrary().GetRayTracingPipeline(pipelineID);
	SPT_CHECK(!!pipeline);

	m_pipelineState.BindRayTracingPipeline(lib::Ref(pipeline));

	GetCommandBufferRHI().BindRayTracingPipeline(pipeline->GetRHI());
}

void CommandRecorder::Dispatch(const math::Vector3u& groupCount)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!m_pipelineState.GetBoundComputePipeline());

	m_pipelineState.FlushDirtyDSForComputePipeline(GetCommandBufferRHI());

	GetCommandBufferRHI().Dispatch(groupCount);
}

void CommandRecorder::DispatchIndirect(const lib::SharedRef<Buffer>& indirectArgsBuffer, Uint64 indirectArgsOffset)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!m_pipelineState.GetBoundComputePipeline());

	m_pipelineState.FlushDirtyDSForComputePipeline(GetCommandBufferRHI());

	GetCommandBufferRHI().DispatchIndirect(indirectArgsBuffer->GetRHI(), indirectArgsOffset);
}

void CommandRecorder::DispatchIndirect(const BufferView& indirectArgsBufferView, Uint64 indirectArgsOffset)
{
	DispatchIndirect(indirectArgsBufferView.GetBuffer(), indirectArgsBufferView.GetOffset() + indirectArgsOffset);
}

void CommandRecorder::TraceRays(const math::Vector3u& traceCount)
{
	SPT_PROFILER_FUNCTION();

	const lib::SharedPtr<RayTracingPipeline>& boundPipeline = m_pipelineState.GetBoundRayTracingPipeline();
	SPT_CHECK(!!boundPipeline);

	m_pipelineState.FlushDirtyDSForRayTracingPipeline(GetCommandBufferRHI());

	GetCommandBufferRHI().TraceRays(boundPipeline->GetShaderBindingTable(), traceCount);
}

void CommandRecorder::BindDescriptorSetState(const lib::MTHandle<DescriptorSetState>& state)
{
	SPT_PROFILER_FUNCTION();

	m_pipelineState.BindDescriptorSetState(state);
}

void CommandRecorder::UnbindDescriptorSetState(const lib::MTHandle<DescriptorSetState>& state)
{
	SPT_PROFILER_FUNCTION();

	m_pipelineState.UnbindDescriptorSetState(state);
}

void CommandRecorder::ExecuteCommands(const lib::SharedRef<rdr::GPUWorkload>& workload)
{
	SPT_PROFILER_FUNCTION();

	const lib::SharedPtr<rdr::CommandBuffer> cmdBufferToExecute = workload->GetRecordedBuffer();
	GetCommandBufferRHI().ExecuteCommands(cmdBufferToExecute->GetRHI());
}

void CommandRecorder::BlitTexture(const lib::SharedRef<Texture>& source, Uint32 sourceMipLevel, Uint32 sourceArrayLayer, const lib::SharedRef<Texture>& dest, Uint32 destMipLevel, Uint32 destArrayLayer, rhi::ETextureAspect aspect, rhi::ESamplerFilterType filterMode)
{
	SPT_PROFILER_FUNCTION();

	GetCommandBufferRHI().BlitTexture(source->GetRHI(), sourceMipLevel, sourceArrayLayer, dest->GetRHI(), destMipLevel, destArrayLayer, aspect, filterMode);
}

void CommandRecorder::ClearTexture(const lib::SharedRef<Texture>& texture, const rhi::ClearColor& clearColor, const rhi::TextureSubresourceRange& subresourceRange)
{
	SPT_PROFILER_FUNCTION();

	GetCommandBufferRHI().ClearTexture(texture->GetRHI(), clearColor, subresourceRange);
}

void CommandRecorder::CopyTexture(const lib::SharedRef<Texture>& source, const rhi::TextureCopyRange& sourceRange, const lib::SharedRef<Texture>& target, const rhi::TextureCopyRange& targetRange, const math::Vector3u& extent)
{
	SPT_PROFILER_FUNCTION();

	GetCommandBufferRHI().CopyTexture(source->GetRHI(), sourceRange, target->GetRHI(), targetRange, extent);
}

void CommandRecorder::CopyBuffer(const lib::SharedRef<Buffer>& sourceBuffer, Uint64 sourceOffset, const lib::SharedRef<Buffer>& destBuffer, Uint64 destOffset, Uint64 size)
{
	SPT_PROFILER_FUNCTION();

	GetCommandBufferRHI().CopyBuffer(sourceBuffer->GetRHI(), sourceOffset, destBuffer->GetRHI(), destOffset, size);
}

void CommandRecorder::FillBuffer(const lib::SharedRef<Buffer>& buffer, Uint64 offset, Uint64 range, Uint32 data)
{
	SPT_PROFILER_FUNCTION();

	GetCommandBufferRHI().FillBuffer(buffer->GetRHI(), offset, range, data);
}

void CommandRecorder::CopyBufferToTexture(const lib::SharedRef<Buffer>& buffer, Uint64 bufferOffset, const lib::SharedRef<Texture>& texture, rhi::ETextureAspect aspect, math::Vector3u copyExtent, math::Vector3u copyOffset /*= math::Vector3u::Zero()*/, Uint32 mipLevel /*= 0*/, Uint32 arrayLayer /*= 0*/)
{
	SPT_PROFILER_FUNCTION();

	GetCommandBufferRHI().CopyBufferToTexture(buffer->GetRHI(), bufferOffset, texture->GetRHI(), aspect, copyExtent, copyOffset, mipLevel, arrayLayer);
}

void CommandRecorder::InitializeUIFonts()
{
	SPT_PROFILER_FUNCTION();

	UIBackend::GetRHI().InitializeFonts(GetCommandBufferRHI());
}

void CommandRecorder::RenderUI()
{
	SPT_PROFILER_FUNCTION();

	UIBackend::GetRHI().Render(GetCommandBufferRHI());
}

void CommandRecorder::BeginDebugRegion(const lib::HashedString& name, const lib::Color& color)
{
	SPT_PROFILER_FUNCTION();

	GetCommandBufferRHI().BeginDebugRegion(name, color);
}

void CommandRecorder::EndDebugRegion()
{
	SPT_PROFILER_FUNCTION();

	GetCommandBufferRHI().EndDebugRegion();
}

#if SPT_ENABLE_GPU_CRASH_DUMPS
void CommandRecorder::SetDebugCheckpoint(const lib::HashedString& marker)
{
	SPT_PROFILER_FUNCTION();

	GetCommandBufferRHI().SetDebugCheckpoint(reinterpret_cast<const void*>(marker.GetKey()));
}

#endif // SPT_ENABLE_GPU_CRASH_DUMPS

void CommandRecorder::ResetQueryPool(const lib::SharedRef<QueryPool>& queryPool, Uint32 firstQueryIdx, Uint32 queryCount)
{
	SPT_PROFILER_FUNCTION();

	GetCommandBufferRHI().ResetQueryPool(queryPool->GetRHI(), firstQueryIdx, queryCount);
}

void CommandRecorder::WriteTimestamp(const lib::SharedRef<QueryPool>& queryPool, Uint32 queryIdx, rhi::EPipelineStage stage)
{
	SPT_PROFILER_FUNCTION();

	GetCommandBufferRHI().WriteTimestamp(queryPool->GetRHI(), queryIdx, stage);
}

void CommandRecorder::BeginQuery(const lib::SharedRef<QueryPool>& queryPool, Uint32 queryIdx)
{
	SPT_PROFILER_FUNCTION();

	GetCommandBufferRHI().BeginQuery(queryPool->GetRHI(), queryIdx);
}

void CommandRecorder::EndQuery(const lib::SharedRef<QueryPool>& queryPool, Uint32 queryIdx)
{
	SPT_PROFILER_FUNCTION();

	GetCommandBufferRHI().EndQuery(queryPool->GetRHI(), queryIdx);
}

rhi::RHICommandBuffer& CommandRecorder::GetCommandBufferRHI() const
{
	return m_commandsBuffer->GetRHI();
}

} // spt::rdr