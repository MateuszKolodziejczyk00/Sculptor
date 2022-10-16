#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rdr
{

class CommandBuffer;
class Context;


struct CommandExecuteContext
{
public:

	explicit CommandExecuteContext(lib::SharedRef<Context> renderContext);

	~CommandExecuteContext();

	Context& GetRenderContext() const;

private:

	const lib::SharedRef<Context> m_renderContext;
};


template<typename TType>
concept CRenderCommand = requires(TType instance)
{
	{ instance(std::declval<const lib::SharedRef<CommandBuffer>>(), std::declval<const CommandExecuteContext&>()) } -> std::same_as<void>;
};


class RenderCommandBase abstract
{
public:

	explicit RenderCommandBase(Uint32 commandSize)
		: m_commandSize(commandSize)
	{ }

	virtual ~RenderCommandBase() = default;

	virtual void Execute(const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext) = 0;

	Uint32 GetCommandSize() const
	{
		return m_commandSize;
	}

private:

	Uint32 m_commandSize;
};


template<CRenderCommand CommandInstance>
class RenderCommand : public RenderCommandBase
{
public:

	using RenderCommandType = RenderCommand<CommandInstance>;

	explicit RenderCommand(CommandInstance&& instance)
		: RenderCommandBase(sizeof(RenderCommandType))
		, m_command(std::forward<CommandInstance>(instance))
	{ }

	// Begin RenderCommandBase overrides
	virtual void Execute(const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext) override
	{
		std::invoke(m_command, cmdBuffer, executionContext);
	}
	// End RenderCommandBase overrides

private:

	CommandInstance	m_command;
};


class CommandQueueIterator
{
public:

	CommandQueueIterator();
	CommandQueueIterator(Byte* commandBuffer, Uint32 commandsNum);

	void				Set(Byte* commandBuffer, Uint32 commandsNum);

	Bool				IsValid() const;

	void				Advance();

	RenderCommandBase*	Get() const;

private:

	Byte*	m_currentCommandPtr;

	/** This also counts current command */
	Uint32	m_remainingCommandsNum;

	Uint32	m_currentCommandSize;
};


enum class ECommandQueueExecuteFlags
{
	None				= 0,
	DestroyCommands		= BIT(0)
};


class CommandQueueExecutor
{
public:

	explicit CommandQueueExecutor(lib::SharedRef<CommandBuffer> cmdBuffer, lib::SharedRef<Context> renderContext);

	void Execute(CommandQueueIterator commandIterator, ECommandQueueExecuteFlags flags);

protected:

	/** May be made virtual if necessary */
	void ExecuteCommand(RenderCommandBase& command, const lib::SharedRef<CommandBuffer>& cmdBuffer, CommandExecuteContext& context);

private:

	lib::SharedRef<CommandBuffer>	m_commandBuffer;

	CommandExecuteContext			m_context;
};


class CommandQueue
{
public:

	CommandQueue();
	~CommandQueue();

	template<CRenderCommand CommandInstance>
	void Enqueue(CommandInstance&& command)
	{
		using CommandType = RenderCommand<CommandInstance>;

		SPT_CHECK(m_currentBufferOffset + sizeof(CommandType) <= m_buffer.size());

		new (m_buffer.data() + m_currentBufferOffset) CommandType(std::forward<CommandInstance>(command));

		m_currentBufferOffset += sizeof(CommandType);
		++m_commandsNum;
	}

	void Execute(CommandQueueExecutor& executor);
	void ExecuteAndReset(CommandQueueExecutor& executor);

	void Reset();

	Bool HasPendingCommands() const;

private:

	void AcquireBufferMemory();
	void ReleaseBufferMemory();

	lib::DynamicArray<Byte>	m_buffer;
	SizeType				m_currentBufferOffset;

	Uint32					m_commandsNum;
};

} // spt::rdr