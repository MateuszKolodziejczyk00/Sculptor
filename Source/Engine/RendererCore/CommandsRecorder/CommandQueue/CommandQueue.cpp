#include "CommandQueue.h"

namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Utils =========================================================================================

namespace utils
{

class CommandQueuesMemoryPool
{
public:
	
	static void AcquireBufferMemory(lib::DynamicArray<Byte>& outBuffer)
	{
		GetInstance().AcquireBufferMemoryImpl(outBuffer);
	}

	static void ReleaseBufferMemory(lib::DynamicArray<Byte>&& buffer)
	{
		GetInstance().ReleaseBufferMemoryImpl(std::forward<lib::DynamicArray<Byte>>(buffer));
	}

private:

	CommandQueuesMemoryPool() = default;

	static CommandQueuesMemoryPool& GetInstance()
	{
		static CommandQueuesMemoryPool instance;
		return instance;
	}

	void AcquireBufferMemoryImpl(lib::DynamicArray<Byte>& outBuffer);
	void ReleaseBufferMemoryImpl(lib::DynamicArray<Byte>&& buffer);

	static constexpr SizeType CommandsQueueBufferSize = 1024 * 1024;

	lib::DynamicArray<lib::DynamicArray<Byte>>		m_availableBuffers;

	/** use spinlock because most of the time, it will block single move operation(but from time to time may also block new buffer allocation) */ 
	lib::Spinlock	m_buffersLock;
};

void CommandQueuesMemoryPool::AcquireBufferMemoryImpl(lib::DynamicArray<Byte>& outBuffer)
{
	const lib::LockGuard lockGuard(m_buffersLock);

	if (!m_availableBuffers.empty())
	{
		outBuffer = std::move(m_availableBuffers.back());
		m_availableBuffers.pop_back();
	}
	else
	{
		outBuffer = lib::DynamicArray<Byte>(CommandsQueueBufferSize);
	}
}

void CommandQueuesMemoryPool::ReleaseBufferMemoryImpl(lib::DynamicArray<Byte>&& buffer)
{
	const lib::LockGuard lockGuard(m_buffersLock);

	m_availableBuffers.emplace_back(std::forward<lib::DynamicArray<Byte>>(buffer));
}

} // utils

//////////////////////////////////////////////////////////////////////////////////////////////////
// CommandExecuteContext =========================================================================

CommandExecuteContext::CommandExecuteContext(lib::SharedRef<Context> renderContext)
	: m_renderContext(std::move(renderContext))
{ }

CommandExecuteContext::~CommandExecuteContext() = default;

Context& CommandExecuteContext::GetRenderContext() const
{
	return *m_renderContext;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CommandQueueIterator ==========================================================================

CommandQueueIterator::CommandQueueIterator()
	: m_currentCommandPtr(nullptr)
	, m_remainingCommandsNum(0)
	, m_currentCommandSize(0)
{ }

CommandQueueIterator::CommandQueueIterator(Byte* commandBuffer, Uint32 commandsNum)
	: m_currentCommandPtr(nullptr)
	, m_remainingCommandsNum(0)
	, m_currentCommandSize(0)
{
	Set(commandBuffer, commandsNum);
}

void CommandQueueIterator::Set(Byte* commandBuffer, Uint32 commandsNum)
{
	m_currentCommandPtr		= commandBuffer;
	m_remainingCommandsNum	= commandsNum;
	
	if (m_remainingCommandsNum > 0)
	{
		const RenderCommandBase* firstCommand = Get();
		SPT_CHECK(!!firstCommand);

		m_currentCommandSize = firstCommand->GetCommandSize();
	}
	else
	{
		m_currentCommandSize = 0;
	}
}

Bool CommandQueueIterator::IsValid() const
{
	return m_remainingCommandsNum > 0;
}

void CommandQueueIterator::Advance()
{
	SPT_CHECK(IsValid());

	m_currentCommandPtr += m_currentCommandSize;
	--m_remainingCommandsNum;

	const RenderCommandBase* currentCommand = Get();
	m_currentCommandSize = currentCommand ? currentCommand->GetCommandSize() : 0;
}

RenderCommandBase* CommandQueueIterator::Get() const
{
	return IsValid() ? reinterpret_cast<RenderCommandBase*>(m_currentCommandPtr) : nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CommandQueueExecutor ==========================================================================

CommandQueueExecutor::CommandQueueExecutor(lib::SharedRef<CommandBuffer> cmdBuffer, lib::SharedRef<Context> renderContext)
	: m_commandBuffer(std::move(cmdBuffer))
	, m_context(std::move(renderContext))
{ }

void CommandQueueExecutor::Execute(CommandQueueIterator commandIterator, ECommandQueueExecuteFlags flags)
{
	SPT_PROFILER_FUNCTION();

	while (commandIterator.IsValid())
	{
		RenderCommandBase* currentCommand = commandIterator.Get();

		ExecuteCommand(*currentCommand, m_commandBuffer, m_context);

		if (lib::HasAnyFlag(flags, ECommandQueueExecuteFlags::DestroyCommands))
		{
			currentCommand->~RenderCommandBase();
		}

		commandIterator.Advance();
	}
}

void CommandQueueExecutor::ExecuteCommand(RenderCommandBase& command, const lib::SharedRef<CommandBuffer>& cmdBuffer, CommandExecuteContext& context)
{
	command.Execute(cmdBuffer, context);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CommandQueue ==================================================================================

CommandQueue::CommandQueue()
	: m_currentBufferOffset(0)
	, m_commandsNum(0)
{
	AcquireBufferMemory();
}

CommandQueue::~CommandQueue()
{
	ReleaseBufferMemory();
}

void CommandQueue::Execute(CommandQueueExecutor& executor)
{
	SPT_PROFILER_FUNCTION();

	executor.Execute(CommandQueueIterator(m_buffer.data(), m_commandsNum), ECommandQueueExecuteFlags::None);
}

void CommandQueue::ExecuteAndReset(CommandQueueExecutor& executor)
{
	SPT_PROFILER_FUNCTION();

	executor.Execute(CommandQueueIterator(m_buffer.data(), m_commandsNum), ECommandQueueExecuteFlags::DestroyCommands);
	m_commandsNum = 0;
	m_currentBufferOffset = 0;
}

void CommandQueue::Reset()
{
	SPT_PROFILER_FUNCTION();

	CommandQueueIterator commandIterator(m_buffer.data(), m_commandsNum);

	// Call destructor for all commands
	while (commandIterator.IsValid())
	{
		commandIterator.Get()->~RenderCommandBase();

		commandIterator.Advance();
	}

	m_commandsNum = 0;
	m_currentBufferOffset = 0;
}

Bool CommandQueue::HasPendingCommands() const
{
	return m_commandsNum > 0;
}

void CommandQueue::AcquireBufferMemory()
{
	SPT_PROFILER_FUNCTION();

	utils::CommandQueuesMemoryPool::AcquireBufferMemory(m_buffer);
}

void CommandQueue::ReleaseBufferMemory()
{
	SPT_PROFILER_FUNCTION();

	if (HasPendingCommands())
	{
		Reset();
	}

	utils::CommandQueuesMemoryPool::ReleaseBufferMemory(std::move(m_buffer));
}

} // spt::rdr