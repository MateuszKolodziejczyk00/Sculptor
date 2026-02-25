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

DataBase* g_dbInstance = nullptr;

} // db

void HashedStringDB::Initialize()
{
	SPT_CHECK(!db::g_dbInstance);
	db::g_dbInstance = new db::DataBase();
}

void HashedStringDB::InitializeModule(HashedStringDBData& db)
{
	SPT_CHECK(!db::g_dbInstance);
	db::g_dbInstance = reinterpret_cast<db::DataBase*>(&db);
}

HashedStringDBData* HashedStringDB::GetDBData()
{
	return reinterpret_cast<HashedStringDBData*>(db::g_dbInstance);
}

HashedStringDB::KeyType HashedStringDB::GetRecord(String&& inString, StringView& outView)
{
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

		outView = CreateRecord(key, std::move(newRecord));
	}

	return key;
}

HashedStringDB::KeyType HashedStringDB::GetRecord(StringView inString, StringView& outView)
{
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

		outView = CreateRecord(key, std::move(newRecord));
	}

	return key;
}

StringView HashedStringDB::GetRecordStringChecked(KeyType key)
{
	StringView outString;
	const Bool found = FindRecord(key, outString);
	SPT_CHECK(found);

	return outString;
}

Bool HashedStringDB::FindRecord(KeyType key, StringView& outView)
{
	const ReadLockGuard readRecordsLock(db::g_dbInstance->recordsMutex);

	const auto foundRecord = db::g_dbInstance->records.find(key);
	if (foundRecord != db::g_dbInstance->records.cend())
	{
		outView = foundRecord->second;

		return true;
	}

	return false;
}

String& HashedStringDB::CreateRecord(KeyType key, String&& newRecord)
{
	const WriteLockGuard addRecordLock(db::g_dbInstance->recordsMutex);

	String& recordRef = db::g_dbInstance->records[key];
	if (recordRef.empty())
	{
		recordRef = std::move(newRecord);
	}

	return recordRef;
}

}
