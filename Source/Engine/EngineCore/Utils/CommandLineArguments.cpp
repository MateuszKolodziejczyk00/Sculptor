#include "CommandLineArguments.h"

#include <regex>

namespace spt::engn
{

void CommandLineArguments::Parse(lib::Span<const lib::StringView> args)
{
	const std::regex equalsRegex("=");

	for (SizeType i = 0; i < args.size(); ++i)
	{
		const lib::String argument(args[i]);

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
