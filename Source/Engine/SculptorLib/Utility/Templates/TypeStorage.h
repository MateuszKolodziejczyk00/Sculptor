#pragma once

#include "SculptorAliases.h"


namespace spt::lib
{

template<typename TType>
struct TypeStorage
{
public:

	TypeStorage() = default;

	template<typename... TArgs>
	TypeStorage(TArgs&&... args)
	{
		Construct(std::forward<TArgs>(args)...);
	}

	template<typename... TArgs>
	void Construct(TArgs&&... args)
	{
		new (m_storage) TType(std::forward<TArgs>(args)...);
	}

	void Destroy()
	{
		Get().~TType();
	}

	TType& Get()
	{
		return *reinterpret_cast<TType*>(m_storage);
	}
	
	const TType& Get() const
	{
		return *reinterpret_cast<const TType*>(m_storage);
	}

	TType&& Move()
	{
		return std::move(*reinterpret_cast<TType*>(m_storage));
	}

	TType* GetAddress()
	{
		return reinterpret_cast<TType*>(m_storage);
	}

	const TType* GetAddress() const
	{
		return reinterpret_cast<const TType*>(m_storage);
	}

	SizeType GetSize() const
	{
		return sizeof(TType);
	}

private:

	alignas(alignof(TType)) Byte m_storage[sizeof(TType)];
};

} // spt::lib