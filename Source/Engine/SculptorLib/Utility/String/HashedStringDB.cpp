#include "HashedStringDB.h"
#include "Containers/HashMap.h"
#include "ProfilerCore.h"
#include "Utility/Threading/Lock.h"
#include "Assertions/Assertions.h"


namespace spt::lib
{

namespace db
{

class DataBase
{
public:

	DataBase()
	{
		records.reserve(4096);
	}

	ReadWriteLock								recordsMutex;
	HashMap<HashedStringDB::KeyType, String>	records;
};

static DataBase instance;

} // db

HashedStringDB::KeyType HashedStringDB::GetRecord(String&& inString, StringView& outView)
{
	SPT_PROFILER_FUNCTION();

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
	SPT_PROFILER_FUNCTION();

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

StringView HashedStringDB::GetRecordStringChecked(KeyType key)
{
	SPT_PROFILER_FUNCTION();

	StringView outString;
	const Bool found = FindRecord(key, outString);
	SPT_CHECK(found);

	return outString;
}

Bool HashedStringDB::FindRecord(KeyType key, StringView& outView)
{
	SPT_PROFILER_FUNCTION();

	const ReadLockGuard readRecordsLock(db::instance.recordsMutex);

	const auto foundRecord = db::instance.records.find(key);
	if (foundRecord != db::instance.records.cend())
	{
		outView = foundRecord->second;

		return true;
	}

	return false;
}

void HashedStringDB::CreateRecord(KeyType key, String&& newRecord)
{
	SPT_PROFILER_FUNCTION();

	const WriteLockGuard addRecordLock(db::instance.recordsMutex);

	db::instance.records.emplace(key, std::forward<String>(newRecord));
}

}