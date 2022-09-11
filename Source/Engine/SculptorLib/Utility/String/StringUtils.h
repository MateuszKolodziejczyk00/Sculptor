#pragma once

#include "SculptorLibMacros.h"
#include "MathCore.h"
#include "String.h"


namespace spt::lib
{

class SCULPTORLIB_API StringUtils
{
public:

	static String		ToHexString(const Byte* data, SizeType size);

	static WString		ToWideString(lib::StringView view);
};

} // spt::lib
