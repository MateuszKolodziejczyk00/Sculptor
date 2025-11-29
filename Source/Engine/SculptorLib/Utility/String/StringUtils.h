#pragma once

#include "SculptorLibMacros.h"
#include "SculptorAliases.h"
#include "String.h"
#include "Containers/Span.h"


namespace spt::lib
{

// Based on Maël Nison answer from https://stackoverflow.com/questions/23999573/convert-a-number-to-a-string-literal-with-constexpr
namespace priv
{
    template<unsigned... digits>
    struct ToCharacters
    {
        static constexpr char value[] = { ('0' + digits)..., '\0'};
    };

    template<unsigned rem, unsigned... digits>
    struct ToDigits : ToDigits<rem / 10, rem % 10, digits...> {};

    template<unsigned... digits>
    struct ToDigits<0, digits...> : ToCharacters<digits...> {};
}


class SCULPTOR_LIB_API StringUtils
{
public:

	static SizeType		ComputeHexSize(SizeType bytesNum);
	static void		    ToHexString(const Byte* data, SizeType size, lib::Span<char> outString);
	static String		ToHexString(const Byte* data, SizeType size);

    static void         FromHexString(lib::Span<const char> string, lib::Span<Byte> outBytes);

	static WString		ToWideString(lib::StringView view);
	static String		ToMultibyteString(lib::WStringView view);

	static String		ToLower(lib::StringView view);

	static Bool IsWhiteChar(const char c);

    template<Uint32 number>
	static constexpr String ToString();
};


template<Uint32 number>
inline constexpr String StringUtils::ToString()
{
    return String(priv::ToDigits<number>::value);
}

} // spt::lib
