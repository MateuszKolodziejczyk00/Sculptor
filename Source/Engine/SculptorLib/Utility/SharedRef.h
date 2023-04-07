#pragma once

#include "Memory.h"
#include "UtilityMacros.h"


namespace spt::lib
{

/**
 * Based on Unreal Engine implementation
 * All Shared refs must be valid
 * Shared refs can be reassigned
 * move assigned swaps references
 * move constructor copies value
 */
template<typename TType>
class SharedRef
{
public:

	explicit SharedRef(SharedPtr<TType> value)
		: m_storage(std::move(value))
	{
		SPT_CHECK(!!m_storage);
	}

	template<typename TRhsType>
		requires std::is_base_of_v<TType, TRhsType>
	explicit SharedRef(const SharedPtr<TRhsType>& value)
		: m_storage(value)
	{
		SPT_CHECK(!!m_storage);
	}

	template<typename TRhsType>
		requires std::is_base_of_v<TType, TRhsType>
	SharedRef(const SharedRef<TRhsType>& value)
		: m_storage(std::move(value.ToSharedPtr()))
	{
		SPT_CHECK(!!m_storage);
	}

	explicit SharedRef(TType* value)
		: m_storage(value)
	{
		SPT_CHECK(!!m_storage);
	}

	SharedRef(const SharedRef<TType>& value)	= default;
	SharedRef<TType>& operator=(const SharedRef<TType>& rhs)	= default;

	SharedRef(SharedRef<TType>&& value)
	{
		*this = value;
	}

	SharedRef<TType>& operator=(SharedRef<TType>&& rhs)
	{
		std::swap(m_storage, rhs.m_storage);
		return *this;
	}

	SPT_NODISCARD decltype(auto) operator->() const
	{
		return m_storage.get();
	}

	SPT_NODISCARD decltype(auto) operator*() const
	{
		return *m_storage;
	}

	SPT_NODISCARD Bool operator==(const SharedRef<TType>& rhs) const
	{
		return m_storage == rhs.m_storage;
	}
	
	SPT_NODISCARD Bool operator!=(const SharedRef<TType>& rhs) const
	{
		return !(*this == rhs);
	}

	SPT_NODISCARD const SharedPtr<TType>& ToSharedPtr() const
	{
		return m_storage;
	}

	SPT_NODISCARD TType* Get() const
	{
		return m_storage.get();
	}

	template<typename TOtherType> requires std::is_base_of_v<TOtherType, TType>
	SPT_NODISCARD operator SharedPtr<TOtherType>() const
	{
		return ToSharedPtr();
	}

private:

	SharedPtr<TType>	m_storage;
};


template<typename TType>
SharedRef<TType> ToSharedRef(lib::SharedPtr<TType> data)
{
	return SharedRef<TType>(std::move(data));
}

template<typename TType, typename... TArgs>
SharedRef<TType> MakeShared(TArgs&&... args)
{
	return SharedRef<TType>(std::make_shared<TType>(std::forward<TArgs>(args)...));
}

template<typename TType>
SharedRef<TType> Ref(lib::SharedPtr<TType> ptr)
{
	return SharedRef<TType>(ptr);
}

} // spt::lib