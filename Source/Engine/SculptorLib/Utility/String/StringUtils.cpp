#include "StringUtils.h"
#include "ProfilerCore.h"
#include <algorithm>

namespace spt::lib
{

String StringUtils::ToHexString(const Byte* data, SizeType size)
{
	SPT_PROFILER_FUNCTION();

	static constexpr char dictionary[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

	const SizeType stringSize = size * 2;

	lib::String result;
	result.reserve(stringSize);

	for (SizeType i = 0; i < size; ++i)
	{
		const SizeType dictIdx1 = static_cast<SizeType>(data[i] & Byte(0x0f));
		const SizeType dictIdx2 = static_cast<SizeType>((data[i] >> 4) & Byte(0x0f));
		result += dictionary[dictIdx1];
		result += dictionary[dictIdx2];
	}

	return result;
}

WString StringUtils::ToWideString(lib::StringView view)
{
	SPT_PROFILER_FUNCTION();

	WString wideString(view.size() + 1, L'\0'); // add 1 to create slot for '\0' character

	SizeType converted = 0;
	mbstowcs_s(&converted, wideString.data(), wideString.size(), view.data(), view.size());

	return wideString;
}

String StringUtils::ToMultibyteString(lib::WStringView view)
{
	String mutlibyteString(view.size() + 1, '\0');

	SizeType converted = 0;
	wcstombs_s(&converted, mutlibyteString.data(), mutlibyteString.size(), view.data(), view.size());

	return mutlibyteString;
}

lib::String StringUtils::ToLower(lib::StringView view)
{
	SPT_PROFILER_FUNCTION();

	lib::String result(view.size(), '\0');

	std::transform(view.begin(), view.end(), result.begin(), [](char c) { return static_cast<char>(std::tolower(c)); });

	return result;
}

} // spt::lib
