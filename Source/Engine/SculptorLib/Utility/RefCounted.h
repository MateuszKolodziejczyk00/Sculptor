#pragma once

#include "SculptorLibMacros.h"
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
		, m_deleteOnZeroRefCount(true)
	{ }

	virtual ~RefCounted()
	{
		SPT_CHECK(m_refCount == 0);
	}

	Bool ShouldDeleteOnZeroRefCount() const
	{
		return m_deleteOnZeroRefCount;
	}

	void DisableDeleteOnZeroRefCount()
	{
		m_deleteOnZeroRefCount = false;
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

	Bool m_deleteOnZeroRefCount;
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

	Handle(const Handle& other)
		: m_object(other.Get())
	{
		if (IsValid())
		{
			AddRef();
		}
	}
	
	Handle(Handle&& other)
		: m_object(other.Get())
	{
		other.ResetWithoutRelease();
	}

	template<typename TInstanceType>
	Handle(const Handle<TInstanceType, TRefCountedType>& other)
		: m_object(other.Get())
	{
		static_assert(ValidateObjectType<TInstanceType>());

		if (IsValid())
		{
			AddRef();
		}
	}

	template<typename TInstanceType>
	Handle(Handle<TInstanceType, TRefCountedType>&& other)
		: m_object(other.Get())
	{
		static_assert(ValidateObjectType<TInstanceType>());
		other.ResetWithoutRelease();
	}

	Handle(nullptr_t)
		: m_object(nullptr)
	{ }

	~Handle()
	{
		ResetWithRelease();
	}

	Bool IsValid() const
	{
		return m_object != nullptr;
	}

	template<typename TInstanceType>
	Handle& operator=(TInstanceType* object)
	{
		static_assert(ValidateObjectType<TInstanceType>());

		ResetWithRelease();

		SetObject<true>(object);

		return *this;
	}

	Handle& operator=(const Handle& other)
	{
		return (*this = other.Get());
	}

	Handle& operator=(Handle&& other)
	{
		ResetWithRelease();

		SetObject<false>(other.Get());
		other.ResetWithoutRelease();

		return *this;
	}

	template<typename TInstanceType>
	Handle& operator=(const Handle<TInstanceType, TRefCountedType>& other)
	{
		return (*this = other.Get());
	}

	template<typename TInstanceType>
	Handle& operator=(Handle<TInstanceType, TRefCountedType>&& other)
	{
		static_assert(ValidateObjectType<TInstanceType>());

		ResetWithRelease();

		SetObject<false>(other.Get());
		other.ResetWithoutRelease();

		return *this;
	}

	Bool operator==(const Handle& other) const
	{
		return m_object == other.m_object;
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
		TRefCountedType* refCountedObj = (TRefCountedType*)m_object;
		if (refCountedObj->Release())
		{
			if (refCountedObj->ShouldDeleteOnZeroRefCount())
			{
				delete refCountedObj;
			}
			else
			{
				refCountedObj->~TRefCountedType();
			}
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

	template<typename THandleInstanceType, typename THandleRefCountedType>
	friend class Handle;
};

} // priv


class SCULPTOR_LIB_API MTRefCounted : public priv::RefCounted<std::atomic<Uint32>> {};
class SCULPTOR_LIB_API RefCounted : public priv::RefCounted<Uint32> {};

template<typename TObject>
using MTHandle = priv::Handle<TObject, MTRefCounted>;

template<typename TObject>
using Handle = priv::Handle<TObject, RefCounted>;

} // spt::lib