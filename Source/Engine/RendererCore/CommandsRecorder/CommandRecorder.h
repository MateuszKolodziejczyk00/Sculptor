#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIBridge/RHICommandBufferImpl.h"
#include "CommandQueue/CommandQueue.h"
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
class Texture;


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

	void									RecordCommands(const lib::SharedRef<Context>& context, const CommandsRecordingInfo& recordingInfo, const rhi::CommandBufferUsageDefinition& commandBufferUsage);

	const lib::SharedPtr<CommandBuffer>&	GetCommandBuffer() const;

	void									ExecuteBarrier(Barrier barrier);

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
	m_commandQueue.Enqueue(std::move(command));
}

} // spt::rdr
