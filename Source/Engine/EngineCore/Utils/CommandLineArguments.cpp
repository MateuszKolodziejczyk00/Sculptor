#include "CommandLineArguments.h"

#include <regex>

namespace spt::engn
{

CommandLineArguments::CommandLineArguments(Uint32 argsNum, char** arguments)
{
	Parse(argsNum, arguments);
}

void CommandLineArguments::Parse(Uint32 argsNum, char** arguments)
{
	const std::regex equalsRegex("=");

	for (Uint32 i = 0; i < argsNum; ++i)
	{
		const lib::String argument = arguments[i];

		auto argumentRegexIt = std::sregex_token_iterator(std::cbegin(argument), std::cend(argument), equalsRegex, -1);

		const lib::String argName = *argumentRegexIt;
		lib::String argValue;

		argumentRegexIt++;
		if (argumentRegexIt != std::sregex_token_iterator())
		{
			argValue = *argumentRegexIt;
			argumentRegexIt++;
		}

		SPT_CHECK(argumentRegexIt == std::sregex_token_iterator());

		m_args.emplace(argName, argValue);
	}
}

Bool CommandLineArguments::Contains(lib::HashedString arg) const
{
	return m_args.find(arg) != std::cend(m_args);
}

lib::HashedString CommandLineArguments::GetValue(lib::HashedString arg) const
{
	const auto foundArg = m_args.find(arg);
	return foundArg != std::cend(m_args) ? foundArg->second : lib::HashedString();
}

} // spt::engn
