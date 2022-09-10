#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rdr
{

class CommandBuffer;


struct CommandExecuteContext
{
	CommandExecuteContext() = default;

	// ...
};

class RenderCommandBase abstract
{
public:

	RenderCommandBase(Uint32 commandSize)
		: m_commandSize(commandSize)
	{ }

	~RenderCommandBase() = default;

	virtual void Execute(const lib::SharedRef<CommandBuffer>& cmdBuffer, CommandExecuteContext& context) = 0;

	Uint32 GetCommandSize() const
	{
		return m_commandSize;
	}

private:

	Uint32 m_commandSize;
};


template<typename CommandInstance>
class RenderCommand : public RenderCommandBase
{
public:

	using RenderCommandType = RenderCommand<CommandInstance>;

	RenderCommand(CommandInstance&& instance)
		: RenderCommandBase(sizeof(RenderCommandType))
		, m_command(std::forward<CommandInstance>(instance))
	{ }

	// Begin RenderCommandBase overrides
	virtual void Execute(const lib::SharedRef<CommandBuffer>& cmdBuffer, CommandExecuteContext& context) override
	{
		std::invoke(m_command, cmdBuffer, context);
	}
	// End RenderCommandBase overrides

private:

	CommandInstance	m_command;
};


class CommandsQueueExecutor
{
public:

	CommandsQueueExecutor(lib::SharedRef<CommandBuffer> cmdBuffer);

	void Execute(Byte* commandBuffer, Uint32 commandsNum);

protected:

	/** May be made virtual if neceessary */
	void ExecuteCommand(RenderCommandBase& command, const lib::SharedRef<CommandBuffer>& cmdBuffer, CommandExecuteContext& context);

private:

	lib::SharedRef<CommandBuffer>	m_commandBuffer;

	// for now this is only a placeholder for future use
	CommandExecuteContext			m_context;
};


class CommandQueue
{
public:

	CommandQueue();
	~CommandQueue();

	template<typename CommandInstance>
	void Enqueue(CommandInstance&& command)
	{
		using CommandType = RenderCommand<CommandInstance>;

		SPT_CHECK(m_currentBufferOffset + sizeof(CommandType) <= m_buffer.size());

		new (m_buffer.data() + m_currentBufferOffset) CommandType(std::forward<CommandInstance>(command));

		m_currentBufferOffset += sizeof(CommandType);
		++m_commandsNum;
	}

	void Execute(CommandsQueueExecutor& executor);
	void ExecuteAndReset(CommandsQueueExecutor& executor);

	void Reset();

private:

	void AcquireBufferMemory();
	void ReleaseBufferMemory();

	lib::DynamicArray<Byte>	m_buffer;
	SizeType				m_currentBufferOffset;

	Uint32					m_commandsNum;
};

} // spt::rdr