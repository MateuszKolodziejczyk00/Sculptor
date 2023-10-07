#pragma once

#include "SculptorAliases.h"
#include <atomic>
#include "Assertions/Assertions.h"
#include <type_traits>

namespace spt::lib
{

namespace priv
{

template<typename TCountType>
class RefCounted
{
public:

	RefCounted()
		: m_refCount(0)
	{ }

	virtual ~RefCounted()
	{
		SPT_CHECK(m_refCount == 0);
	}

	void AddRef()
	{
		++m_refCount;
	}

	Bool Release()
	{
		SPT_CHECK(m_refCount > 0);

		return (--m_refCount == 0);
	}

	const TCountType& GetRefCount() const
	{
		return m_refCount;
	}

private:

	TCountType m_refCount;
};


template<typename TObject, typename TRefCountedType>
class Handle
{
public:

	Handle()
		: m_object(nullptr)
	{ }

	template<typename TInstanceType>
	Handle(TInstanceType* object)
		: m_object(object)
	{

		static_assert(ValidateObjectType<TInstanceType>());

		if (IsValid())
		{
			AddRef();
		}
	}

	Handle(nullptr_t)
		: m_object(nullptr)
	{ }

	Handle(const Handle& other)
		: m_object(other.m_object)
	{
		if (IsValid())
		{
			AddRef();
		}
	}

	Handle(Handle&& other)
		: m_object(other.m_object)
	{
		other.ResetWithoutRelease();
	}

	~Handle()
	{
		ResetWithRelease();
	}

	Bool IsValid() const
	{
		return m_object != nullptr;
	}

	Handle& operator=(const Handle& other)
	{
		ResetWithRelease();

		SetObject<true>(other.m_object);

		return *this;
	}

	Handle& operator=(Handle&& other)
	{
		ResetWithRelease();

		SetObject<false>(other.m_object);
		other.ResetWithoutRelease();

		return *this;
	}

	template<typename TInstanceType>
	Handle& operator=(TInstanceType* object)
	{
		static_assert(ValidateObjectType<TInstanceType>());

		ResetWithRelease();

		SetObject<true>(object);

		return *this;
	}

	void Reset()
	{
		ResetWithRelease();
	}

	TObject* Get() const
	{
		return m_object;
	}

	TObject* operator->() const
	{
		return m_object;
	}

	TObject& operator*() const
	{
		return *m_object;
	}

private:

	void AddRef() const
	{
		((TRefCountedType*)m_object)->AddRef();
	}

	void Release() const
	{
		if (((TRefCountedType*)m_object)->Release())
		{
			delete m_object;
		}
	}

	void ResetWithoutRelease()
	{
		m_object = nullptr;
	}
	
	void ResetWithRelease()
	{
		if (m_object)
		{
			Release();
			m_object = nullptr;
		}
	}

	template<Bool addRef>
	void SetObject(TObject* inObject)
	{
		m_object = inObject;
		if constexpr (addRef)
		{
			if (IsValid())
			{
				AddRef();
			}
		}
	}

	template<typename TInstanceType>
	static constexpr Bool ValidateObjectType()
	{
		return std::is_base_of<TObject, TInstanceType>::value
			|| std::is_same<TRefCountedType, TObject>::value;
	}

	TObject* m_object;
};

} // priv


using MTRefCounted = priv::RefCounted<std::atomic<Uint32>>;
using RefCounted = priv::RefCounted<Uint32>;

template<typename TObject>
using MTHandle = priv::Handle<TObject, MTRefCounted>;

template<typename TObject>
using Handle = priv::Handle<TObject, RefCounted>;

} // spt::lib