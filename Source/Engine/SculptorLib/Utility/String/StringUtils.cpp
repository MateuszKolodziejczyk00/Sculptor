#include "StringUtils.h"
#include "ProfilerCore.h"
#include <algorithm>
#include "Assertions/Assertions.h"

namespace spt::lib
{

SizeType StringUtils::ComputeHexSize(SizeType bytesNum)
{
	return bytesNum * 2;
}

void StringUtils::ToHexString(const Byte* data, SizeType size, lib::Span<char> outString)
{
	static constexpr char dictionary[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

	SPT_CHECK(ComputeHexSize(size) <= outString.size());

	for (SizeType i = 0; i < size; ++i)
	{
		const SizeType dictIdx1 = static_cast<SizeType>(data[i] & Byte(0x0f));
		const SizeType dictIdx2 = static_cast<SizeType>((data[i] >> 4) & Byte(0x0f));

		outString[i * 2]     = dictionary[dictIdx1];
		outString[i * 2 + 1] = dictionary[dictIdx2];
	}
}

String StringUtils::ToHexString(const Byte* data, SizeType size)
{
	lib::String result;
	result.resize(ComputeHexSize(size));

	ToHexString(data, size, { result.data(), result.size() });

	return result;
}

void StringUtils::FromHexString(lib::Span<const char> string, lib::Span<Byte> outBytes)
{
	SPT_CHECK(ComputeHexSize(outBytes.size()) == string.size());

	const auto dictionary = [](char c) -> Byte
	{
		switch (c)
		{
		case '0': return static_cast<Byte>(0x0);
		case '1': return static_cast<Byte>(0x1);
		case '2': return static_cast<Byte>(0x2);
		case '3': return static_cast<Byte>(0x3);
		case '4': return static_cast<Byte>(0x4);
		case '5': return static_cast<Byte>(0x5);
		case '6': return static_cast<Byte>(0x6);
		case '7': return static_cast<Byte>(0x7);
		case '8': return static_cast<Byte>(0x8);
		case '9': return static_cast<Byte>(0x9);
		case 'A': return static_cast<Byte>(0xA);
		case 'B': return static_cast<Byte>(0xB);
		case 'C': return static_cast<Byte>(0xC);
		case 'D': return static_cast<Byte>(0xD);
		case 'E': return static_cast<Byte>(0xE);
		case 'F': return static_cast<Byte>(0xF);
		default: SPT_CHECK_NO_ENTRY(); return static_cast<Byte>(0x0);
		}

	};

	for (SizeType i = 0u; i < string.size(); i += 2u)
	{
		const Byte byte = dictionary(string[i]) | (dictionary(string[i + 1u]) << 4u);

		outBytes[i / 2u] = byte;
	}
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
