#pragma once

#include "SculptorLibMacros.h"
#include "HashedStringDB.h"


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

	Bool IsValid() const
	{
		return key != DataBaseType::keyNone;
	}

	void Reset()
	{
		key = DataBaseType::keyNone;
		stringView = {};
	}

	StringView GetString() const
	{
		return stringView;
	}

	const char* GetData() const
	{
		return stringView.data();
	}

	KeyType GetKey() const
	{
		return key;
	}

private:

	KeyType		key;
	StringView	stringView;
};


using HashedString = HashedStringBase<HashedStringDB>;

}