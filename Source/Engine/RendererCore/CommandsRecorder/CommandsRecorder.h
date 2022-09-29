#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "CommandQueue/CommandQueue.h"
#include "RHICore/RHICommandBufferTypes.h"
#include "Pipelines/PipelineState.h"
#include "PipelinePendingState.h"
#include "RendererUtils.h"


namespace spt::rdr
{

class CommandBuffer;
class Barrier;
class RenderingDefinition;
class UIBackend;
class Context;
class DescriptorSetState;


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


class RENDERER_CORE_API CommandsRecorder
{
public:

	CommandsRecorder();
	~CommandsRecorder();

	Bool	IsBuildingCommands() const;
	Bool	IsRecording() const;
	Bool	IsPending() const;

	void									RecordCommands(const lib::SharedRef<Context>& context, const CommandsRecordingInfo& recordingInfo, const rhi::CommandBufferUsageDefinition& commandBufferUsage);

	const lib::SharedPtr<CommandBuffer>&	GetCommandsBuffer() const;

	void									ExecuteBarrier(Barrier barrier);

	void									BeginRendering(const RenderingDefinition& definition);
	void									EndRendering();

	void									BindGraphicsPipeline(PipelineStateID pipelineID);

	void									BindComputePipeline(PipelineStateID pipelineID);

	void									BindDescriptorSetState(const lib::SharedRef<DescriptorSetState>& state);
	void									UnbindDescriptorSetState(const lib::SharedRef<DescriptorSetState>& state);

	void									InitializeUIFonts(const lib::SharedRef<rdr::UIBackend>& uiBackend);

	void									RenderUI(const lib::SharedRef<rdr::UIBackend>& uiBackend);

private:

	template<CRenderCommand RenderCommand>
	void EnqueueRenderCommand(RenderCommand&& command);

	lib::SharedPtr<CommandBuffer>			m_commandsBuffer;

	CommandQueue							m_commandQueue;

	ECommandsRecorderState					m_state;

	PipelinePendingState					m_pipelineState;
};


template<CRenderCommand RenderCommand>
void CommandsRecorder::EnqueueRenderCommand(RenderCommand&& command)
{
	m_commandQueue.Enqueue(std::move(command));
}

} // spt::rdr
