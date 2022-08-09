#include "HashedStringDB.h"
#include "Containers/HashMap.h"
#include "Profiler.h"
#include "Mutex.h"


namespace spt::lib
{

namespace priv
{

static ReadWriteLock recordsMutex;

static HashMap<HashedStringDB::KeyType, String> records;

}

HashedStringDB::KeyType HashedStringDB::GetRecord(String&& inString, StringView& outView)
{
	SPT_PROFILE_FUNCTION();

	const KeyType key = HashString(inString);

	const Bool foundRecord = FindRecord(key, outView);

	if (!foundRecord)
	{
		// We need to disable SSO (small string optimization) in these strings
		// otherwise string_views in HashedString may point invalid memory after reallocation of records after reallocation of records
		constexpr SizeType requiredSizeToDisableSSO = sizeof(String) + 1;

		String newRecord;
		if (inString.size() > requiredSizeToDisableSSO)
		{
			newRecord = std::forward<String>(inString);
		}
		else
		{
			newRecord.reserve(requiredSizeToDisableSSO);
			newRecord = inString;
		}

		outView = newRecord;

		CreateRecord(key, std::move(newRecord));
	}

	return key;
}

HashedStringDB::KeyType HashedStringDB::GetRecord(StringView inString, StringView& outView)
{
	SPT_PROFILE_FUNCTION();

	const KeyType key = HashString(inString);

	const Bool foundRecord = FindRecord(key, outView);

	if (!foundRecord)
	{
		// We need to disable SSO (small string optimization) in these strings
		// otherwise string_views in HashedString may point invalid memory after reallocation of records after reallocation of records
		constexpr SizeType requiredSizeToDisableSSO = sizeof(String) + 1;

		String newRecord(inString);
		if (inString.size() < requiredSizeToDisableSSO)
		{
			newRecord.reserve(requiredSizeToDisableSSO);
		}

		outView = newRecord;

		CreateRecord(key, std::move(newRecord));
	}

	return key;
}

HashedStringDB::KeyType HashedStringDB::HashString(StringView string)
{
	return std::hash<std::string_view>()(string);
}

Bool HashedStringDB::FindRecord(KeyType key, StringView& outView)
{
	SPT_PROFILE_FUNCTION();

	const ReadLockGuard readRecordsLock(priv::recordsMutex);

	const auto foundRecord = priv::records.find(key);
	if (foundRecord != priv::records.cend())
	{
		outView = foundRecord->second;

		return true;
	}

	return false;
}

void HashedStringDB::CreateRecord(KeyType key, String&& newRecord)
{
	SPT_PROFILE_FUNCTION();

	const WriteLockGuard addRecordLock(priv::recordsMutex);

	priv::records.emplace(key, std::forward<String>(newRecord));
}

}