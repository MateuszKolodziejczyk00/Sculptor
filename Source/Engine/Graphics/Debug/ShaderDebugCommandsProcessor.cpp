#include "ShaderDebugCommandsProcessor.h"
#include "ShaderDebugCommandsStream.h"
#include "ShaderDebugCommandsExecutor.h"


namespace spt::gfx::dbg
{

SPT_DEFINE_LOG_CATEGORY(ShaderDebugCommandsProcessor, true)


ShaderDebugCommandsProcessor::ShaderDebugCommandsProcessor(lib::SharedRef<rdr::Buffer> commands, lib::SharedRef<rdr::Buffer> size)
	: m_commands(std::move(commands))
	, m_size(std::move(size))
{ }

void ShaderDebugCommandsProcessor::BindExecutors(lib::DynamicArray<lib::SharedPtr<ShaderDebugCommandsExecutor>> executors)
{
	m_executors = std::move(executors);
}

void ShaderDebugCommandsProcessor::ProcessCommands()
{
	SPT_PROFILER_FUNCTION();

	ShaderDebugCommandsStream commandsStream(m_commands, m_size);

	while (!commandsStream.Ended())
	{
		const Uint32 opCode = commandsStream.Read<Uint32>();

		const auto executorIt = std::find_if(m_executors.begin(), m_executors.end(),
											 [opCode](const lib::SharedPtr<ShaderDebugCommandsExecutor>& executor)
											 {
												 return executor->CanExecute(opCode);
											 });

		if (executorIt != m_executors.end())
		{
			(*executorIt)->Execute(opCode, commandsStream);
		}
		else
		{
			SPT_LOG_ERROR(ShaderDebugCommandsProcessor, "There's no executor for Op Code '{0}'", opCode);
		}
	}
}

} // spt::gfx::dbg
