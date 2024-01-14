#include "ShaderPrintfExecutor.h"
#include "ShaderDebugCommandsStream.h"
#include "Utility/Templates/Overload.h"


namespace spt::gfx::dbg
{

SPT_DEFINE_LOG_CATEGORY(ShaderPrintf, true)


Bool ShaderPrintfExecutor::CanExecute(Uint32 opCode) const
{
	return opCode == shader_command_op::Printf;
}

void ShaderPrintfExecutor::Execute(Uint32 opCode, ShaderDebugCommandsStream& commandsStream)
{
	ExecutePrintf(commandsStream);
}

void ShaderPrintfExecutor::ExecutePrintf(ShaderDebugCommandsStream& commandsStream) const
{
	SPT_PROFILER_FUNCTION();

	const Uint64 literalHash = commandsStream.Read<Uint64>();

	static_assert(sizeof(Uint64) == sizeof(typename lib::HashedString::KeyType));

	const lib::HashedString literal(static_cast<typename lib::HashedString::KeyType>(literalHash));

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


	SPT_LOG_INFO(ShaderPrintf, string.c_str());
}

} // spt::gfx::dbg
