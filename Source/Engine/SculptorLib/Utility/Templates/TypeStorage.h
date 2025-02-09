#pragma once

#include "SculptorAliases.h"


namespace spt::lib
{

template<SizeType alignment, SizeType size>
struct GenericStorage
{
public:

	constexpr SizeType GetSize() const
	{
		return size;
	}

	constexpr SizeType GetAlignment() const
	{
		return alignment;
	}

	GenericStorage() = default;

	Byte* GetAddress()
	{
		return m_storage;
	}

	const Byte* GetAddress() const
	{
		return m_storage;
	}

private:

	alignas(alignment) Byte m_storage[size];
};


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
		new (m_genericStorage.GetAddress()) TType(std::forward<TArgs>(args)...);
	}

	void Destroy()
	{
		Get().~TType();
	}

	TType& Get()
	{
		return *reinterpret_cast<TType*>(m_genericStorage.GetAddress());
	}
	
	const TType& Get() const
	{
		return *reinterpret_cast<const TType*>(m_genericStorage.GetAddress());
	}

	TType&& Move()
	{
		return std::move(*reinterpret_cast<TType*>(m_genericStorage.GetAddress()));
	}

	TType* GetAddress()
	{
		return reinterpret_cast<TType*>(m_genericStorage.GetAddress());
	}

	const TType* GetAddress() const
	{
		return reinterpret_cast<const TType*>(m_genericStorage.GetAddress());
	}

	SizeType GetSize() const
	{
		return sizeof(TType);
	}

private:

	GenericStorage<alignof(TType), sizeof(TType)> m_genericStorage;
};

} // spt::lib