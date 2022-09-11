#pragma once

#include "SculptorLibMacros.h"
#include "HashedStringDB.h"
#include "Utility/UtilityMacros.h"


namespace spt::lib
{

template<typename TDataBase>
class HashedStringBase
{
public:

	using DataBaseType	= TDataBase;
	using KeyType		= typename DataBaseType::KeyType;
	using ThisType		= HashedStringBase<DataBaseType>;

	HashedStringBase()
		: key(DataBaseType::keyNone)
	{ }

	HashedStringBase(const ThisType& rhs)
		: key(rhs.key)
		, stringView(rhs.stringView)
	{ }

	HashedStringBase(StringView rhs)
	{
		key = DataBaseType::GetRecord(rhs, stringView);
	}

	HashedStringBase(const String& rhs)
	{
		key = DataBaseType::GetRecord(lib::StringView(rhs), stringView);
	}

	HashedStringBase(String&& rhs)
	{
		key = DataBaseType::GetRecord(std::forward<String>(rhs), stringView);
	}

	HashedStringBase(const char* rhs)
	{
		key = DataBaseType::GetRecord(StringView(rhs), stringView);
	}

	ThisType& operator=(const ThisType& rhs)
	{
		key = rhs.key;
		stringView = rhs.stringView;
		return *this;
	}

	ThisType& operator=(StringView rhs)
	{
		key = DataBaseType::GetRecord(rhs, stringView);
		return *this;
	}

	ThisType& operator=(const String& rhs)
	{
		key = DataBaseType::GetRecord(StringView(rhs), stringView);
		return *this;
	}

	ThisType& operator=(String&& rhs)
	{
		key = DataBaseType::GetRecord(std::forward<String>(rhs), stringView);
		return *this;
	}

	ThisType& operator=(const char* rhs)
	{
		key = DataBaseType::GetRecord(StringView(rhs), stringView);
		return *this;
	}

	Bool operator==(const ThisType& rhs) const
	{
		return key == rhs.key;
	}

	Bool operator==(StringView rhs) const
	{
		return key == DataBaseType::HashString(rhs);
	}

	Bool operator!=(const ThisType& rhs) const
	{
		return !(*this == rhs);
	}

	Bool operator!=(StringView rhs) const
	{
		return !(*this == rhs);
	}

	SPT_NODISCARD Bool IsValid() const
	{
		return key != DataBaseType::keyNone;
	}

	void Reset()
	{
		key = DataBaseType::keyNone;
		stringView = {};
	}

	SPT_NODISCARD StringView GetView() const
	{
		return stringView;
	}

	SPT_NODISCARD lib::String ToString() const
	{
		return lib::String(GetView());
	}

	SPT_NODISCARD const char* GetData() const
	{
		return stringView.data();
	}

	SPT_NODISCARD SizeType GetSize() const
	{
		return stringView.size();
	}

	SPT_NODISCARD KeyType GetKey() const
	{
		return key;
	}

private:

	KeyType		key;
	StringView	stringView;
};


using HashedString = HashedStringBase<HashedStringDB>;

}


namespace std
{

template<>
struct hash<spt::lib::HashedString>
{

	size_t operator()(spt::lib::HashedString string) const
	{
		static_assert(std::is_convertible_v<spt::lib::HashedString::KeyType, size_t>, "Type of key must be convertible to size_t");
		return static_cast<size_t>(string.GetKey());
	}

};

}