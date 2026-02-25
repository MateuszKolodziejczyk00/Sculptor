#pragma once

#include "SculptorLibMacros.h"
#include "Utility/UtilityMacros.h"
#include "SculptorAliases.h"
#include "String.h"
#include "Utility/Hash.h"


namespace spt::lib
{

struct HashedStringDBData {};

class SCULPTOR_LIB_API HashedStringDB
{
public:

	using KeyType = SizeType;

	static constexpr KeyType keyNone = idxNone<KeyType>;

	static void                Initialize();
	static void                InitializeModule(HashedStringDBData& db);
	static HashedStringDBData* GetDBData();

	static KeyType GetRecord(String&& inString, StringView& outView);
	static KeyType GetRecord(StringView inString, StringView& outView);

	static StringView GetRecordStringChecked(KeyType key);

	SPT_NODISCARD static constexpr KeyType HashString(StringView string)
	{
		return GetHash(string);
	}

private:

	static Bool FindRecord(KeyType key, StringView& outView);

	static String& CreateRecord(KeyType key, String&& newRecord);
};

} // spt::lib
