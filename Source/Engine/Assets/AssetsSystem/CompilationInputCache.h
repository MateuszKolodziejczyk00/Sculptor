#pragma once

#include "SculptorCoreTypes.h"


namespace spt::as
{

class CompilationInputCache
{
public:

	CompilationInputCache() = default;

	template<typename TType, typename TLoader>
	const TType& GetOrLoadData(const lib::HashedString& key, TLoader&& loader)
	{
		lib::LockGuard lockGuard(m_lock);

		const auto foundDataIt = m_cachedData.find(key);
		if (foundDataIt != m_cachedData.cend())
		{
			const BaseEntry* entry = foundDataIt->second.get();
			SPT_CHECK(lib::RuntimeTypeInfo(lib::TypeInfo<TType>()) == entry->type);
			return reinterpret_cast<const Entry<TType>*>(entry)->data;
		}

		lib::UniquePtr<Entry<TType>> newEntry = lib::MakeUnique<Entry<TType>>();
		newEntry->type = lib::TypeInfo<TType>();
		newEntry->data = loader();

		const TType& data = newEntry->data;

		m_cachedData[key] = std::move(newEntry);

		return data;
	}

	void OnCompilationStarted()
	{
		// Lock is unfortunately necessary because we might be currently releasing data after last compilations batch
		lib::LockGuard lockGuard(m_lock);

		++m_compilationsInProgressNum;
	}

	void OnCompilationFinished()
	{
		lib::LockGuard lockGuard(m_lock);

		SPT_CHECK(m_compilationsInProgressNum > 0u);

		if (--m_compilationsInProgressNum == 0u)
		{
			m_cachedData.clear();
		}
	}

private:

	lib::Lock m_lock;

	Uint32 m_compilationsInProgressNum = 0u;

	struct BaseEntry
	{
		virtual ~BaseEntry() = default;
		lib::RuntimeTypeInfo type;
	};

	template<typename TType>
	struct Entry : public BaseEntry
	{
		TType data;
	};

	lib::HashMap<lib::HashedString, lib::UniquePtr<BaseEntry>> m_cachedData;
};

} // spt::as
