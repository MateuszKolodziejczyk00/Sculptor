#pragma once

#include "SculptorLibMacros.h"
#include "MathCore.h"
#include "String.h"


namespace spt::lib
{

class SCULPTORLIB_API HashedStringDB
{
public:

	using KeyType = SizeType;

	static constexpr KeyType keyNone = idxNone<KeyType>;

	static KeyType GetRecord(String&& inString, StringView& outView);
	static KeyType GetRecord(StringView inString, StringView& outView);

	static KeyType HashString(StringView string);

private:

	static Bool FindRecord(KeyType key, StringView& outView);

	static void CreateRecord(KeyType key, String&& newRecord);
};

}