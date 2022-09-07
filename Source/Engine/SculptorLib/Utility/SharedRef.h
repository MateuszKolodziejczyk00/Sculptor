#pragma once

#include "Memory.h"
#include "UtilityMacros.h"


namespace spt::lib
{

template<typename TType>
class SharedRef
{
public:

	SharedRef(SharedPtr<TType> value)
		: m_storage(std::move(value))
	{
		SPT_CHECK(!!m_storage);
	}

	template<typename TRhsType>
		requires std::is_base_of_v<TType, TRhsType>
	SharedRef(const SharedPtr<TRhsType>& value)
		: m_storage(value)
	{
		SPT_CHECK(!!m_storage);
	}

	SharedRef(const SharedRef<TType>& value) = default;
	SharedRef(SharedRef<TType>&& value) = default;

	SharedRef<TType>& operator=(const SharedRef<TType>& rhs)	= delete;
	SharedRef<TType>& operator=(SharedRef<TType>&& rhs)			= delete;

	SPT_NODISCARD decltype(auto) operator->() const
	{
		return m_storage.get();
	}

	SPT_NODISCARD decltype(auto) operator*() const
	{
		return *m_storage;
	}

	SPT_NODISCARD const SharedPtr<TType>& ToSharedPtr() const
	{
		return m_storage;
	}

	SPT_NODISCARD operator SharedPtr<TType>() const
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

} // spt::lib