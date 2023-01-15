#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIBridge/RHICommandBufferImpl.h"
#include "CommandQueue/CommandQueue.h"
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


struct CommandsRecordingInfo
{
	RendererResourceName				commandsBufferName;
	rhi::CommandBufferDefinition		commandBufferDef;
};


enum class ECommandsRecorderState
{
	/** Recording didn't started */
	BuildingCommands,
	/** Currently recording */
	Recording,
	/** Finished recording */
	Pending
};


class RENDERER_CORE_API CommandRecorder
{
public:

	CommandRecorder();
	~CommandRecorder();

	Bool	IsBuildingCommands() const;
	Bool	IsRecording() const;
	Bool	IsPending() const;

	void									RecordCommands(const lib::SharedRef<RenderContext>& context, const CommandsRecordingInfo& recordingInfo, const rhi::CommandBufferUsageDefinition& commandBufferUsage);

	const lib::SharedPtr<CommandBuffer>&	GetCommandBuffer() const;

	void									ExecuteBarrier(rhi::RHIDependency dependency);

	void									SetEvent(const lib::SharedRef<Event>& event, rhi::RHIDependency dependency);
	void									WaitEvent(const lib::SharedRef<Event>& event, rhi::RHIDependency dependency);

	void									BeginRendering(const RenderingDefinition& definition);
	void									EndRendering();
	
	void									SetViewport(const math::AlignedBox2f& renderingViewport, Real32 minDepth, Real32 maxDepth);
	void									SetScissor(const math::AlignedBox2u& renderingScissor);

	void									DrawIndirect(const lib::SharedRef<Buffer>& drawsBuffer, Uint64 drawsOffset, Uint32 drawsStride, const lib::SharedRef<Buffer>& countBuffer, Uint64 countOffset, Uint32 maxDrawsCount);
	void									DrawIndirect(const BufferView& drawsBufferView, Uint64 drawsOffset, Uint32 drawsStride, const BufferView& countBufferView, Uint64 countOffset, Uint32 maxDrawsCount);

	void									BindGraphicsPipeline(PipelineStateID pipelineID);
	void									BindGraphicsPipeline(const rhi::GraphicsPipelineDefinition& pipelineDef, const ShaderID& shader);

	/** This version lets us cache created pipeline. If doesn't attempt to create new pipeline if cached id is valid */
	void									BindGraphicsPipeline(const rhi::GraphicsPipelineDefinition& pipelineDef, const ShaderID& shader, INOUT PipelineStateID& cachedPipelineID);

	void									BindComputePipeline(PipelineStateID pipelineID);
	void									BindComputePipeline(const ShaderID& shader);

	void									Dispatch(const math::Vector3u& groupCount);

	void									BindDescriptorSetState(const lib::SharedRef<DescriptorSetState>& state);
	void									UnbindDescriptorSetState(const lib::SharedRef<DescriptorSetState>& state);

	template<typename TDescriptorSetStatesRange>
	void									BindDescriptorSetStates(TDescriptorSetStatesRange&& states);

	template<typename TDescriptorSetStatesRange>
	void									UnbindDescriptorSetStates(TDescriptorSetStatesRange&& states);

	void									CopyTexture(const lib::SharedRef<Texture>& source, const rhi::TextureCopyRange& sourceRange, const lib::SharedRef<Texture>& target, const rhi::TextureCopyRange& targetRange, const math::Vector3u& extent);
	void									CopyBuffer(const lib::SharedRef<Buffer>& sourceBuffer, Uint64 sourceOffset, const lib::SharedRef<Buffer>& destBuffer, Uint64 destOffset, Uint64 size);
	void									FillBuffer(const lib::SharedRef<Buffer>& buffer, Uint64 offset, Uint64 range, Uint32 data);

#if WITH_GPU_CRASH_DUMPS
	void									SetDebugCheckpoint(const lib::HashedString& marker);
#endif // WITH_GPU_CRASH_DUMPS

	void									InitializeUIFonts();

	void									RenderUI();

	// Debug ============================================

	void	BeginDebugRegion(const lib::HashedString& name, const lib::Color& color);
	void	EndDebugRegion();

private:

	template<CRenderCommand RenderCommand>
	void EnqueueRenderCommand(RenderCommand&& command);

	lib::SharedPtr<CommandBuffer>			m_commandsBuffer;

	CommandQueue							m_commandQueue;

	ECommandsRecorderState					m_state;

	PipelinePendingState					m_pipelineState;
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

template<CRenderCommand RenderCommand>
void CommandRecorder::EnqueueRenderCommand(RenderCommand&& command)
{
	SPT_CHECK(IsBuildingCommands());

	m_commandQueue.Enqueue(std::move(command));
}

} // spt::rdr
