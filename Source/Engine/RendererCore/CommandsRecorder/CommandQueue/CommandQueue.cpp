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
// CommandsQueueExecutor =========================================================================

CommandsQueueExecutor::CommandsQueueExecutor(lib::SharedRef<CommandBuffer> cmdBuffer)
	: m_commandBuffer(std::move(cmdBuffer))
{ }

void CommandsQueueExecutor::Execute(Byte* commandBuffer, Uint32 commandsNum)
{
	SPT_PROFILER_FUNCTION();

	Byte* currentCommandPtr = commandBuffer;
	Uint32 remainingCommands = commandsNum;

	while (remainingCommands)
	{
		RenderCommandBase* currentCommand = reinterpret_cast<RenderCommandBase*>(currentCommandPtr);

		ExecuteCommand(*currentCommand, m_commandBuffer, m_context);

		currentCommandPtr += currentCommand->GetCommandSize();
		--remainingCommands;
	}
}

void CommandsQueueExecutor::ExecuteCommand(RenderCommandBase& command, const lib::SharedRef<CommandBuffer>& cmdBuffer, CommandExecuteContext& context)
{
	command.Execute(cmdBuffer, context);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CommandQueue ==================================================================================

CommandQueue::CommandQueue()
	: m_commandsNum(0)
{
	AcquireBufferMemory();
}

CommandQueue::~CommandQueue()
{
	ReleaseBufferMemory();
}

void CommandQueue::Execute(CommandsQueueExecutor& executor)
{
	SPT_PROFILER_FUNCTION();

	executor.Execute(m_buffer.data(), m_commandsNum);
}

void CommandQueue::ExecuteAndReset(CommandsQueueExecutor& executor)
{
	Execute(executor);
	Reset();
}

void CommandQueue::Reset()
{
	m_commandsNum = 0;
	m_currentBufferOffset = 0;
}

void CommandQueue::AcquireBufferMemory()
{
	SPT_PROFILER_FUNCTION();

	utils::CommandQueuesMemoryPool::AcquireBufferMemory(m_buffer);
}

void CommandQueue::ReleaseBufferMemory()
{
	SPT_PROFILER_FUNCTION();

	utils::CommandQueuesMemoryPool::ReleaseBufferMemory(std::move(m_buffer));
}

} // spt::rdr