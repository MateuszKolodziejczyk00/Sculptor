#include "ShaderMessageCommandsExecutor.h"
#include "ShaderDebugCommandsStream.h"
#include "Utility/Templates/Overload.h"


namespace spt::gfx::dbg
{

SPT_DEFINE_LOG_CATEGORY(ShaderMessage, true)


Bool ShaderMessageCommandsExecutor::CanExecute(Uint32 opCode) const
{
	return opCode == shader_command_op::Printf || opCode == shader_command_op::FailedAssertionMessage;
}

void ShaderMessageCommandsExecutor::Execute(Uint32 opCode, ShaderDebugCommandsStream& commandsStream)
{
	switch (opCode)
	{
		case shader_command_op::Printf:
			ExecutePrintf(commandsStream);
			break;

		case shader_command_op::FailedAssertionMessage:
			ExecuteFailedAssertionMessage(commandsStream);
			break;
	}
}

void ShaderMessageCommandsExecutor::ExecutePrintf(ShaderDebugCommandsStream& commandsStream) const
{
	SPT_PROFILER_FUNCTION();

	const lib::String string = ReadString(commandsStream);

	SPT_LOG_INFO(ShaderMessage, string.c_str());
}

void ShaderMessageCommandsExecutor::ExecuteFailedAssertionMessage(ShaderDebugCommandsStream& commandsStream) const
{
	SPT_PROFILER_FUNCTION();

	const lib::HashedString fileName = commandsStream.ReadLiteral();

	const Uint32 lineNumber = commandsStream.Read<Uint32>();

	const lib::String string = ReadString(commandsStream);

	SPT_LOG_ERROR(ShaderMessage, "Assertion failed in file {} at line {}:\n{}", fileName.GetData(), lineNumber, string.c_str());

	SPT_CHECK_NO_ENTRY();
}

lib::String ShaderMessageCommandsExecutor::ReadString(ShaderDebugCommandsStream& commandsStream) const
{
	const lib::HashedString literal = commandsStream.ReadLiteral();

	lib::String string = literal.ToString();

	size_t argumentPosition = string.find("{}", 0);
	while (argumentPosition != std::string::npos)
	{
		const DebugArgument argument = commandsStream.ReadArgument();

		const lib::String argumentString = std::visit(
			lib::Overload
			{
				[](const InvalidArgument& arg) -> lib::String
				{
					return "Invalid argument";
				},
				[](Real32 arg) -> lib::String
				{
					return std::to_string(arg);
				},
				[](Int32 arg) -> lib::String
				{
					return std::to_string(arg);
				},
				[](Uint32 arg) -> lib::String
				{
					return std::to_string(arg);
				},
				[](const auto& arg) -> lib::String
				{
					std::stringstream ss;
					ss << arg.transpose();
					return ss.str();
				}
			},
			argument);

		string.replace(argumentPosition, 2, argumentString);
		argumentPosition = string.find("{}", 0);
	}

	return string;
}

} // spt::gfx::dbg
