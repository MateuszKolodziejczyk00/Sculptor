#pragma once

#include "SculptorLibMacros.h"
#include "SculptorAliases.h"
#include "String.h"


namespace spt::lib
{

class SCULPTORLIB_API StringUtils
{
public:

	static String		ToHexString(const Byte* data, SizeType size);

	static WString		ToWideString(lib::StringView view);
	static String		ToMultibyteString(lib::WStringView view);
};

} // spt::lib
