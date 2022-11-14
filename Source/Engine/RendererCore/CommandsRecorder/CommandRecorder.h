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

	void									BindGraphicsPipeline(PipelineStateID pipelineID);
	void									BindGraphicsPipeline(const rhi::GraphicsPipelineDefinition& pipelineDef, const ShaderID& shader);

	void									BindComputePipeline(PipelineStateID pipelineID);
	void									BindComputePipeline(const ShaderID& shader);

	void									Dispatch(const math::Vector3u& groupCount);

	void									BindDescriptorSetState(const lib::SharedRef<DescriptorSetState>& state);
	void									UnbindDescriptorSetState(const lib::SharedRef<DescriptorSetState>& state);

	void									CopyTexture(const lib::SharedRef<Texture>& source, const rhi::TextureCopyRange& sourceRange, const lib::SharedRef<Texture>& target, const rhi::TextureCopyRange& targetRange, const math::Vector3u& extent);

#if WITH_GPU_CRASH_DUMPS
	void									SetDebugCheckpoint(const void* markerPtr);
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


template<CRenderCommand RenderCommand>
void CommandRecorder::EnqueueRenderCommand(RenderCommand&& command)
{
	SPT_CHECK(IsBuildingCommands());

	m_commandQueue.Enqueue(std::move(command));
}

} // spt::rdr
