#include "CommandRecorder.h"
#include "RenderingDefinition.h"
#include "ResourcesManager.h"
#include "Renderer.h"
#include "Pipelines/PipelinesLibrary.h"
#include "Types/CommandBuffer.h"
#include "Types/UIBackend.h"
#include "Types/Pipeline/GraphicsPipeline.h"
#include "Types/Pipeline/ComputePipeline.h"
#include "Types/GPUEvent.h"
#include "Types/Buffer.h"
#include "Types/AccelerationStructure.h"
#include "Types/QueryPool.h"
#include "Types/DescriptorHeap.h"
#include "DeviceQueues/GPUWorkload.h"


namespace spt::rdr
{

CommandRecorder::CommandRecorder(const rdr::RendererResourceName& name, const lib::SharedRef<RenderContext>& context, const rhi::CommandBufferDefinition& cmdBufferDef, const rhi::CommandBufferUsageDefinition& commandBufferUsage)
{
	m_commandsBuffer = ResourcesManager::CreateCommandBuffer(name, context, cmdBufferDef);
	m_commandsBuffer->StartRecording(commandBufferUsage);

	BindDescriptorHeap(Renderer::GetDescriptorHeap());

	m_canReuseCommandBuffer = !lib::HasAnyFlag(commandBufferUsage.beginFlags, rhi::ECommandBufferBeginFlags::OneTimeSubmit);
}

lib::SharedRef<GPUWorkload> CommandRecorder::FinishRecording()
{
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
	dependency.ExecuteBarrier(GetCommandBufferRHI());
}

void CommandRecorder::SetEvent(const lib::SharedRef<Event>& event, rhi::RHIDependency& dependency)
{
	dependency.SetEvent(GetCommandBufferRHI(), event->GetRHI());
}

void CommandRecorder::WaitEvent(const lib::SharedRef<Event>& event, rhi::RHIDependency& dependency)
{
	dependency.WaitEvent(GetCommandBufferRHI(), event->GetRHI());
}

void CommandRecorder::BindDescriptorHeap(const DescriptorHeap& descriptorHeap)
{
	GetCommandBufferRHI().BindDescriptorHeap(descriptorHeap.GetRHI());
}

void CommandRecorder::BuildBLAS(const lib::SharedRef<BottomLevelAS>& blas, const lib::SharedRef<Buffer>& scratchBuffer, Uint64 scratchBufferOffset)
{
	GetCommandBufferRHI().BuildBLAS(blas->GetRHI(), scratchBuffer->GetRHI(), scratchBufferOffset);
}

void CommandRecorder::BuildTLAS(const lib::SharedRef<TopLevelAS>& tlas, const lib::SharedRef<Buffer>& scratchBuffer, Uint64 scratchBufferOffset, const lib::SharedRef<Buffer>& instancesBuildDataBuffer)
{
	GetCommandBufferRHI().BuildTLAS(tlas->GetRHI(), scratchBuffer->GetRHI(), scratchBufferOffset, instancesBuildDataBuffer->GetRHI());
}

void CommandRecorder::BeginRendering(const RenderingDefinition& definition)
{
	 GetCommandBufferRHI().BeginRendering(definition.GetRHI());
}

void CommandRecorder::EndRendering()
{
	UnbindGraphicsPipeline();

	GetCommandBufferRHI().EndRendering();
}

void CommandRecorder::SetViewport(const math::AlignedBox2f& renderingViewport, Real32 minDepth, Real32 maxDepth)
{
	 GetCommandBufferRHI().SetViewport(renderingViewport, minDepth, maxDepth);
}

void CommandRecorder::SetScissor(const math::AlignedBox2u& renderingScissor)
{
	 GetCommandBufferRHI().SetScissor(renderingScissor);
}

void CommandRecorder::DrawIndirectCount(const lib::SharedRef<Buffer>& drawsBuffer, Uint64 drawsOffset, Uint32 drawsStride, const lib::SharedRef<Buffer>& countBuffer, Uint64 countOffset, Uint32 maxDrawsCount)
{
	m_pipelineState.FlushDirtyDSForGraphicsPipeline(GetCommandBufferRHI());

	GetCommandBufferRHI().DrawIndirectCount(drawsBuffer->GetRHI(), drawsOffset, drawsStride, countBuffer->GetRHI(), countOffset, maxDrawsCount);
}

void CommandRecorder::DrawIndirectCount(const BufferView& drawsBufferView, Uint64 drawsOffset, Uint32 drawsStride, const BufferView& countBufferView, Uint64 countOffset, Uint32 maxDrawsCount)
{
	DrawIndirectCount(drawsBufferView.GetBuffer(), drawsBufferView.GetOffset() + drawsOffset, drawsStride, countBufferView.GetBuffer(), countBufferView.GetOffset() + countOffset, maxDrawsCount);
}

void CommandRecorder::DrawIndirect(const lib::SharedRef<Buffer>& drawsBuffer, Uint64 drawsOffset, Uint32 drawsStride, Uint32 drawsCount)
{
	m_pipelineState.FlushDirtyDSForGraphicsPipeline(GetCommandBufferRHI());

	GetCommandBufferRHI().DrawIndirect(drawsBuffer->GetRHI(), drawsOffset, drawsStride, drawsCount);
}

void CommandRecorder::DrawIndirect(const BufferView& drawsBufferView, Uint64 drawsOffset, Uint32 drawsStride, Uint32 drawsCount)
{
	DrawIndirect(drawsBufferView.GetBuffer(), drawsBufferView.GetOffset() + drawsOffset, drawsStride, drawsCount);
}

void CommandRecorder::DrawInstances(Uint32 verticesNum, Uint32 instancesNum, Uint32 firstVertex /*= 0*/, Uint32 firstInstance /*= 0*/)
{
	m_pipelineState.FlushDirtyDSForGraphicsPipeline(GetCommandBufferRHI());

	GetCommandBufferRHI().DrawInstances(verticesNum, instancesNum, firstVertex, firstInstance);
}

void CommandRecorder::DrawMeshTasks(const math::Vector3u& groupCount)
{
	m_pipelineState.FlushDirtyDSForGraphicsPipeline(GetCommandBufferRHI());

	GetCommandBufferRHI().DrawMeshTasks(groupCount);
}

void CommandRecorder::DrawMeshTasksIndirect(const lib::SharedRef<Buffer>& drawsBuffer, Uint64 drawsOffset, Uint32 drawsStride, Uint32 drawsCount)
{
	m_pipelineState.FlushDirtyDSForGraphicsPipeline(GetCommandBufferRHI());

	GetCommandBufferRHI().DrawMeshTasksIndirect(drawsBuffer->GetRHI(), drawsOffset, drawsStride, drawsCount);
}

void CommandRecorder::DrawMeshTasksIndirectCount(const lib::SharedRef<Buffer>& drawsBuffer, Uint64 drawsOffset, Uint32 drawsStride, const lib::SharedRef<Buffer>& countBuffer, Uint64 countOffset, Uint32 maxDrawsCount)
{
	m_pipelineState.FlushDirtyDSForGraphicsPipeline(GetCommandBufferRHI());

	GetCommandBufferRHI().DrawMeshTasksIndirectCount(drawsBuffer->GetRHI(), drawsOffset, drawsStride, countBuffer->GetRHI(), countOffset, maxDrawsCount);
}

void CommandRecorder::BindGraphicsPipeline(PipelineStateID pipelineID)
{
	const lib::SharedPtr<GraphicsPipeline> pipeline = Renderer::GetPipelinesLibrary().GetGraphicsPipeline(pipelineID);
	SPT_CHECK(!!pipeline);

	m_pipelineState.BindGraphicsPipeline(lib::Ref(pipeline));

	GetCommandBufferRHI().BindGfxPipeline(pipeline->GetRHI());
}

void CommandRecorder::UnbindGraphicsPipeline()
{
	m_pipelineState.UnbindGraphicsPipeline();
}

void CommandRecorder::BindComputePipeline(PipelineStateID pipelineID)
{
	const lib::SharedPtr<ComputePipeline> pipeline = Renderer::GetPipelinesLibrary().GetComputePipeline(pipelineID);
	SPT_CHECK(!!pipeline);

	m_pipelineState.BindComputePipeline(lib::Ref(pipeline));

	GetCommandBufferRHI().BindComputePipeline(pipeline->GetRHI());
}

void CommandRecorder::BindComputePipeline(const ShaderID& shader)
{
	const PipelineStateID pipelineID = Renderer::GetPipelinesLibrary().GetOrCreateComputePipeline(RENDERER_RESOURCE_NAME(shader.GetName()), shader);
	BindComputePipeline(pipelineID);
}

void CommandRecorder::UnbindComputePipeline()
{
	m_pipelineState.UnbindComputePipeline();
}

void CommandRecorder::BindRayTracingPipeline(PipelineStateID pipelineID)
{
	const lib::SharedPtr<RayTracingPipeline> pipeline = Renderer::GetPipelinesLibrary().GetRayTracingPipeline(pipelineID);
	SPT_CHECK(!!pipeline);

	m_pipelineState.BindRayTracingPipeline(lib::Ref(pipeline));

	GetCommandBufferRHI().BindRayTracingPipeline(pipeline->GetRHI());
}

void CommandRecorder::UnbindRayTracingPipeline()
{
	m_pipelineState.UnbindRayTracingPipeline();
}

void CommandRecorder::Dispatch(const math::Vector3u& groupCount)
{
	SPT_CHECK(!!m_pipelineState.GetBoundComputePipeline());

	m_pipelineState.FlushDirtyDSForComputePipeline(GetCommandBufferRHI());

	GetCommandBufferRHI().Dispatch(groupCount);
}

void CommandRecorder::DispatchIndirect(const lib::SharedRef<Buffer>& indirectArgsBuffer, Uint64 indirectArgsOffset)
{
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
	const lib::SharedPtr<RayTracingPipeline>& boundPipeline = m_pipelineState.GetBoundRayTracingPipeline();
	SPT_CHECK(!!boundPipeline);

	m_pipelineState.FlushDirtyDSForRayTracingPipeline(GetCommandBufferRHI());

	GetCommandBufferRHI().TraceRays(boundPipeline->GetShaderBindingTable(), traceCount);
}

void CommandRecorder::TraceRaysIndirect(const lib::SharedRef<Buffer>& indirectArgsBuffer, Uint64 indirectArgsOffset)
{
	const lib::SharedPtr<RayTracingPipeline>& boundPipeline = m_pipelineState.GetBoundRayTracingPipeline();
	SPT_CHECK(!!boundPipeline);

	m_pipelineState.FlushDirtyDSForRayTracingPipeline(GetCommandBufferRHI());

	GetCommandBufferRHI().TraceRaysIndirect(boundPipeline->GetShaderBindingTable(), indirectArgsBuffer->GetRHI(), indirectArgsOffset);
}

void CommandRecorder::TraceRaysIndirect(const BufferView& indirectArgsBufferView, Uint64 indirectArgsOffset)
{
	TraceRaysIndirect(indirectArgsBufferView.GetBuffer(), indirectArgsBufferView.GetOffset() + indirectArgsOffset);
}

void CommandRecorder::BindDescriptorSetState(const lib::MTHandle<DescriptorSetState>& state)
{
	m_pipelineState.BindDescriptorSetState(state);
}

void CommandRecorder::UnbindDescriptorSetState(const lib::MTHandle<DescriptorSetState>& state)
{
	m_pipelineState.UnbindDescriptorSetState(state);
}

void CommandRecorder::ExecuteCommands(const lib::SharedRef<rdr::GPUWorkload>& workload)
{
	const lib::SharedPtr<rdr::CommandBuffer> cmdBufferToExecute = workload->GetRecordedBuffer();
	GetCommandBufferRHI().ExecuteCommands(cmdBufferToExecute->GetRHI());
}

void CommandRecorder::BlitTexture(const lib::SharedRef<Texture>& source, Uint32 sourceMipLevel, Uint32 sourceArrayLayer, const lib::SharedRef<Texture>& dest, Uint32 destMipLevel, Uint32 destArrayLayer, rhi::ETextureAspect aspect, rhi::ESamplerFilterType filterMode)
{
	GetCommandBufferRHI().BlitTexture(source->GetRHI(), sourceMipLevel, sourceArrayLayer, dest->GetRHI(), destMipLevel, destArrayLayer, aspect, filterMode);
}

void CommandRecorder::ClearTexture(const lib::SharedRef<Texture>& texture, const rhi::ClearColor& clearColor, const rhi::TextureSubresourceRange& subresourceRange)
{
	GetCommandBufferRHI().ClearTexture(texture->GetRHI(), clearColor, subresourceRange);
}

void CommandRecorder::CopyTexture(const lib::SharedRef<Texture>& source, const rhi::TextureCopyRange& sourceRange, const lib::SharedRef<Texture>& target, const rhi::TextureCopyRange& targetRange, const math::Vector3u& extent)
{
	GetCommandBufferRHI().CopyTexture(source->GetRHI(), sourceRange, target->GetRHI(), targetRange, extent);
}

void CommandRecorder::CopyBuffer(const lib::SharedRef<Buffer>& sourceBuffer, Uint64 sourceOffset, const lib::SharedRef<Buffer>& destBuffer, Uint64 destOffset, Uint64 size)
{
	GetCommandBufferRHI().CopyBuffer(sourceBuffer->GetRHI(), sourceOffset, destBuffer->GetRHI(), destOffset, size);
}

void CommandRecorder::FillBuffer(const lib::SharedRef<Buffer>& buffer, Uint64 offset, Uint64 range, Uint32 data)
{
	GetCommandBufferRHI().FillBuffer(buffer->GetRHI(), offset, range, data);
}

void CommandRecorder::CopyBufferToTexture(const lib::SharedRef<Buffer>& buffer, Uint64 bufferOffset, const lib::SharedRef<Texture>& texture, rhi::ETextureAspect aspect, math::Vector3u copyExtent, math::Vector3u copyOffset /*= math::Vector3u::Zero()*/, Uint32 mipLevel /*= 0*/, Uint32 arrayLayer /*= 0*/)
{
	GetCommandBufferRHI().CopyBufferToTexture(buffer->GetRHI(), bufferOffset, texture->GetRHI(), aspect, copyExtent, copyOffset, mipLevel, arrayLayer);
}

void CommandRecorder::CopyTextureToBuffer(const lib::SharedRef<Texture>& texture, rhi::ETextureAspect aspect, math::Vector3u copyExtent, math::Vector3u copyOffset, const lib::SharedRef<Buffer>& buffer, Uint64 bufferOffset, Uint32 mipLevel /*= 0*/, Uint32 arrayLayer /*= 0*/)
{
	GetCommandBufferRHI().CopyTextureToBuffer(texture->GetRHI(), aspect, copyExtent, copyOffset, buffer->GetRHI(), bufferOffset, mipLevel, arrayLayer);
}

void CommandRecorder::InitializeUIFonts()
{
	UIBackend::GetRHI().InitializeFonts(GetCommandBufferRHI());
}

void CommandRecorder::RenderUI()
{
	UIBackend::GetRHI().Render(GetCommandBufferRHI());
}

void CommandRecorder::BeginDebugRegion(const lib::HashedString& name, const lib::Color& color)
{
	GetCommandBufferRHI().BeginDebugRegion(name, color);
}

void CommandRecorder::EndDebugRegion()
{
	GetCommandBufferRHI().EndDebugRegion();
}

#if SPT_ENABLE_GPU_CRASH_DUMPS
void CommandRecorder::SetDebugCheckpoint(const lib::HashedString& marker)
{
	GetCommandBufferRHI().SetDebugCheckpoint(reinterpret_cast<const void*>(marker.GetKey()));
}

#endif // SPT_ENABLE_GPU_CRASH_DUMPS

void CommandRecorder::ResetQueryPool(const lib::SharedRef<QueryPool>& queryPool, Uint32 firstQueryIdx, Uint32 queryCount)
{
	GetCommandBufferRHI().ResetQueryPool(queryPool->GetRHI(), firstQueryIdx, queryCount);
}

void CommandRecorder::WriteTimestamp(const lib::SharedRef<QueryPool>& queryPool, Uint32 queryIdx, rhi::EPipelineStage stage)
{
	GetCommandBufferRHI().WriteTimestamp(queryPool->GetRHI(), queryIdx, stage);
}

void CommandRecorder::BeginQuery(const lib::SharedRef<QueryPool>& queryPool, Uint32 queryIdx)
{
	GetCommandBufferRHI().BeginQuery(queryPool->GetRHI(), queryIdx);
}

void CommandRecorder::EndQuery(const lib::SharedRef<QueryPool>& queryPool, Uint32 queryIdx)
{
	GetCommandBufferRHI().EndQuery(queryPool->GetRHI(), queryIdx);
}

rhi::RHICommandBuffer& CommandRecorder::GetCommandBufferRHI() const
{
	return m_commandsBuffer->GetRHI();
}

} // spt::rdr