#include "StringUtils.h"
#include "ProfilerCore.h"

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
		const SizeType dictIdx1 = static_cast<SizeType>(data[i] & 0x0f);
		const SizeType dictIdx2 = static_cast<SizeType>((data[i] >> 4) & 0x0f);
		result += dictionary[dictIdx1];
		result += dictionary[dictIdx2];
	}

	return result;
}

} // spt::lib
