#pragma once

#include "SculptorCoreTypes.h"

namespace spt::lib
{

inline String EncodeBase64(Span<const Byte> data)
{
	const char* base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	lib::Span<const Uint8> data8{ reinterpret_cast<const Uint8*>(data.data()), data.size() };
	
	lib::String encodedString;
	encodedString.reserve(((data8.size() + 2) / 3) * 4);

	size_t i = 0;
	while (i < data8.size())
	{
		 const Uint8 b1 = data8[i++];
		const bool hasB2 = (i < data8.size());
		const Uint8 b2 = hasB2 ? data8[i++] : 0;
		const bool hasB3 = (i < data8.size());
		const Uint8 b3 = hasB3 ? data8[i++] : 0;

		encodedString += base64Chars[(b1 >> 2) & 0x3F];
		encodedString += base64Chars[((b1 & 0x03) << 4) | ((b2 >> 4) & 0x0F)];
		
		if (hasB2 && hasB3)
		{
			encodedString += base64Chars[((b2 & 0x0F) << 2) | ((b3 >> 6) & 0x03)];
			encodedString += base64Chars[b3 & 0x3F];
		}
		else if (hasB2 && !hasB3)
		{
			encodedString += base64Chars[((b2 & 0x0F) << 2)];
			encodedString += '=';
		}
		else
		{
			encodedString += '=';
			encodedString += '=';
		}
	}

	return encodedString;
}

inline DynamicArray<Byte> DecodeBase64(const String& encodedString)
{
	const char* base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	auto decodeChar = [&](char c) -> int
	{
		if (c == '=') return -1;
		const char* p = std::strchr(base64Chars, c);
		return p ? static_cast<int>(p - base64Chars) : -2;
	};

	DynamicArray<Byte> decodedData;
	decodedData.reserve((encodedString.size() / 4) * 3);

	size_t i = 0;
	while (i + 3 < encodedString.size())
	{
		const int c1 = decodeChar(encodedString[i++]);
		const int c2 = decodeChar(encodedString[i++]);
		const int c3 = decodeChar(encodedString[i++]);
		const int c4 = decodeChar(encodedString[i++]);

		if (c1 < 0 || c2 < 0 || c1 == -2 || c2 == -2)
			break;

		const Uint8 b1 = static_cast<Uint8>((c1 << 2) | (c2 >> 4));
		decodedData.push_back(static_cast<Byte>(b1));

		if (c3 >= 0)
		{
			const Uint8 b2 = static_cast<Uint8>(((c2 & 0x0F) << 4) | (c3 >> 2));
			decodedData.push_back(static_cast<Byte>(b2));

			if (c4 >= 0)
			{
				const Uint8 b3 = static_cast<Uint8>(((c3 & 0x03) << 6) | c4);
				decodedData.push_back(static_cast<Byte>(b3));
			}
			else if (c4 == -2)
			{
				break;
			}
		}
		else if (c3 == -2)
		{
			break;
		}
	}
	
	return decodedData;
}

} // spt::lib