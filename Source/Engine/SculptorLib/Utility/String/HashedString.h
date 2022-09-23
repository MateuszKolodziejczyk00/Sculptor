#pragma once

#include "SculptorLibMacros.h"
#include "HashedStringDB.h"
#include "Utility/UtilityMacros.h"


namespace spt::lib
{

namespace impl
{

/**
 * Compile time hash representation.
 * Can be used to create hashes in compile time, and then efficiently compare with hashed strings in runtime
 */
template<typename TDataBase>
class StringHash
{
public:

	using DataBaseType	= TDataBase;
	using KeyType		= typename DataBaseType::KeyType;
	using ThisType		= StringHash<DataBaseType>;

	constexpr StringHash()
		: m_key(DataBaseType::keyNone)
	{ }

	StringHash(const char* literal)
		: m_key(DataBaseType::HashString(literal))
	{ }

	SPT_NODISCARD KeyType GetKey() const
	{
		return m_key;
	}

private:

	KeyType m_key;
};


template<typename TDataBase>
class HashedString
{
public:

	using DataBaseType	= TDataBase;
	using KeyType		= typename DataBaseType::KeyType;
	using ThisType		= HashedString<DataBaseType>;

	HashedString()
		: m_key(DataBaseType::keyNone)
	{ }

	HashedString(const ThisType& rhs)
		: m_key(rhs.m_key)
		, m_stringView(rhs.m_stringView)
	{ }

	HashedString(StringView rhs)
	{
		m_key = DataBaseType::GetRecord(rhs, m_stringView);
	}

	HashedString(const String& rhs)
	{
		m_key = DataBaseType::GetRecord(lib::StringView(rhs), m_stringView);
	}

	HashedString(String&& rhs)
	{
		m_key = DataBaseType::GetRecord(std::forward<String>(rhs), m_stringView);
	}

	HashedString(const char* rhs)
	{
		m_key = DataBaseType::GetRecord(StringView(rhs), m_stringView);
	}

	ThisType& operator=(const ThisType& rhs)
	{
		m_key = rhs.m_key;
		m_stringView = rhs.m_stringView;
		return *this;
	}

	ThisType& operator=(StringView rhs)
	{
		m_key = DataBaseType::GetRecord(rhs, m_stringView);
		return *this;
	}

	ThisType& operator=(const String& rhs)
	{
		m_key = DataBaseType::GetRecord(StringView(rhs), m_stringView);
		return *this;
	}

	ThisType& operator=(String&& rhs)
	{
		m_key = DataBaseType::GetRecord(std::forward<String>(rhs), m_stringView);
		return *this;
	}

	ThisType& operator=(const char* rhs)
	{
		m_key = DataBaseType::GetRecord(StringView(rhs), m_stringView);
		return *this;
	}

	SPT_NODISCARD Bool operator==(const ThisType& rhs) const
	{
		return m_key == rhs.m_key;
	}

	SPT_NODISCARD Bool operator==(StringView rhs) const
	{
		return m_key == DataBaseType::HashString(rhs);
	}

	SPT_NODISCARD Bool operator==(StringHash<DataBaseType> rhs) const
	{
		return m_key == rhs.GetKey();
	}

	SPT_NODISCARD Bool operator!=(const ThisType& rhs) const
	{
		return !(*this == rhs);
	}

	SPT_NODISCARD Bool operator!=(StringView rhs) const
	{
		return !(*this == rhs);
	}

	SPT_NODISCARD Bool operator!=(StringHash<DataBaseType> rhs) const
	{
		return !(*this == rhs);
	}

	SPT_NODISCARD Bool IsValid() const
	{
		return m_key != DataBaseType::keyNone;
	}

	void Reset()
	{
		m_key = DataBaseType::keyNone;
		m_stringView = {};
	}

	SPT_NODISCARD StringView GetView() const
	{
		return m_stringView;
	}

	SPT_NODISCARD lib::String ToString() const
	{
		return lib::String(GetView());
	}

	SPT_NODISCARD const char* GetData() const
	{
		return m_stringView.data();
	}

	SPT_NODISCARD SizeType GetSize() const
	{
		return m_stringView.size();
	}

	SPT_NODISCARD KeyType GetKey() const
	{
		return m_key;
	}

private:

	KeyType		m_key;
	StringView	m_stringView;
};

} // impl

using StringHash	= impl::StringHash<HashedStringDB>;
using HashedString	= impl::HashedString<HashedStringDB>;

} // spt::lib


namespace std
{

template<>
struct hash<spt::lib::StringHash>
{

	size_t operator()(spt::lib::HashedString string) const
	{
		static_assert(std::is_convertible_v<spt::lib::HashedString::KeyType, size_t>, "Type of key must be convertible to size_t");
		return static_cast<size_t>(string.GetKey());
	}

};

template<>
struct hash<spt::lib::HashedString>
{

	size_t operator()(spt::lib::HashedString string) const
	{
		static_assert(std::is_convertible_v<spt::lib::HashedString::KeyType, size_t>, "Type of key must be convertible to size_t");
		return static_cast<size_t>(string.GetKey());
	}

};

} // std