﻿#pragma once


#include "SculptorCoreTypes.h"
#include "Delegate.h"
#include "Utility/ValueGuard.h"

#include <vector>


namespace spt::lib
{

using DelegateIDType = Int32;


struct DelegateHandle
{
public:

	DelegateHandle()
		: id(-1)
	{ }

	DelegateHandle(Int32 inId)
		: id(inId)
	{ }

	Bool IsValid() const
	{
		return id != -1;
	}

	void Reset()
	{
		id = -1;
	}

	Bool operator==(const DelegateHandle& rhs) const
	{
		return id == rhs.id;
	}

	DelegateIDType id;
};


namespace priv
{

template<typename TDelegate>
struct DelegateInfo
{
	DelegateInfo()
	{}

	explicit DelegateInfo(DelegateHandle inHandle)
		: handle(inHandle)
	{}

	TDelegate		delegate;
	DelegateHandle	handle;
};

} // priv


template<Bool isThreadSafe, typename... TArgs>
class MulticastDelegateBase {};


template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
class MulticastDelegateBase<isThreadSafe, TReturnType(TArgs...)> : private DelegateConditionalThreadSafeData<isThreadSafe>
{
public:

	using Delegate = DelegateBase<false, TReturnType(TArgs...)>;

private:

	using DelegateInfo = priv::DelegateInfo<Delegate>;

public:

	using ThisType = MulticastDelegateBase<isThreadSafe, TReturnType(TArgs...)>;

	MulticastDelegateBase()
		: m_handleCounter(1)
	{ }

	MulticastDelegateBase(const ThisType& rhs) = delete;
	ThisType& operator=(const ThisType& rhs) = delete;

	MulticastDelegateBase(ThisType&& rhs) = default;
	ThisType& operator=(ThisType&& rhs) = default;

	MulticastDelegateBase(MulticastDelegateBase<true, TReturnType(TArgs...)>&& rhs) requires !isThreadSafe
	{
		const auto lock = rhs.LockIfNecessary();
		m_delegates = std::move(rhs.m_delegates);
		m_handleCounter = rhs.m_handleCounter;
	}

	MulticastDelegateBase& operator=(MulticastDelegateBase<true, TReturnType(TArgs...)>&& rhs) requires !isThreadSafe
	{
		const auto lock = rhs.LockIfNecessary();
		m_delegates = std::move(rhs.m_delegates);
		m_handleCounter = std::max(rhs.m_handleCounter, m_handleCounter);
		return *this;
	}

	~MulticastDelegateBase() = default;

	DelegateHandle		Add(Delegate delegate);

	template<typename TFuncType, typename... TPayload>
	DelegateHandle		AddRaw(TFuncType function, TPayload&&... payload);

	template<typename TObjectType, typename TFuncType, typename... TPayload>
	DelegateHandle		AddRawMember(TObjectType* object, TFuncType function, TPayload&&... payload);

	template<typename TObjectType, typename TFuncType, typename... TPayload>
	DelegateHandle		AddSharedMember(lib::SharedPtr<TObjectType> object, TFuncType function, TPayload&&... payload);

	template<typename TObjectType, typename TFuncType, typename... TPayload>
	DelegateHandle		AddWeakMember(const lib::SharedPtr<TObjectType>& object, TFuncType function, TPayload&&... payload);

	template<typename TLambda, typename... TPayload>
	DelegateHandle		AddLambda(TLambda&& callable, TPayload&&... payload);

	void				Unbind(DelegateHandle handle);

	void				Reset();
	
	Bool				IsBound() const;

	void				Broadcast(TArgs... arguments);
	void				ResetAndBroadcast(TArgs... arguments);

private:

	using ThreadSafeUtils = DelegateConditionalThreadSafeData<isThreadSafe>;

	lib::DynamicArray<DelegateInfo>		m_delegates;
	DelegateIDType						m_handleCounter;

	Bool								m_deferRemovals;

	friend MulticastDelegateBase<!isThreadSafe, TReturnType(TArgs...)>;
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// Binds =========================================================================================

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
DelegateHandle MulticastDelegateBase<isThreadSafe, TReturnType(TArgs...)>::Add(Delegate delegate)
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	const DelegateHandle handle = m_handleCounter++;
	m_delegates.emplace_back(std::move(DelegateInfo(handle))).delegate = std::move(delegate);
	return handle;
}

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
template<typename TFuncType, typename... TPayload>
DelegateHandle MulticastDelegateBase<isThreadSafe, TReturnType(TArgs...)>::AddRaw(TFuncType function, TPayload&&... payload)
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	const DelegateHandle handle = m_handleCounter++;
	m_delegates.emplace_back(std::move(DelegateInfo(handle))).delegate.BindRaw(function, std::forward<TPayload>(payload)...);
	return handle;
}

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
template<typename TObjectType, typename TFuncType, typename... TPayload>
DelegateHandle MulticastDelegateBase<isThreadSafe, TReturnType(TArgs...)>::AddRawMember(TObjectType* object, TFuncType function, TPayload&&... payload)
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	const DelegateHandle handle = m_handleCounter++;
	m_delegates.emplace_back(std::move(DelegateInfo(handle))).delegate.BindRawMember(object, function, std::forward<TPayload>(payload)...);
	return handle;
}

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
template<typename TObjectType, typename TFuncType, typename... TPayload>
DelegateHandle MulticastDelegateBase<isThreadSafe, TReturnType(TArgs...)>::AddSharedMember(lib::SharedPtr<TObjectType> object, TFuncType function, TPayload&&... payload)
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	const DelegateHandle handle = m_handleCounter++;
	m_delegates.emplace_back(std::move(DelegateInfo(handle))).delegate.BindSharedMember(std::move(object), function, std::forward<TPayload>(payload)...);
	return handle;
}

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
template<typename TObjectType, typename TFuncType, typename... TPayload>
DelegateHandle MulticastDelegateBase<isThreadSafe, TReturnType(TArgs...)>::AddWeakMember(const lib::SharedPtr<TObjectType>& object, TFuncType function, TPayload&&... payload)
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	const DelegateHandle handle = m_handleCounter++;
	m_delegates.emplace_back(std::move(DelegateInfo(handle))).delegate.BindWeakMember(object, function, std::forward<TPayload>(payload)...);
	return handle;
}

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
template<typename TLambda, typename... TPayload>
DelegateHandle MulticastDelegateBase<isThreadSafe, TReturnType(TArgs...)>::AddLambda(TLambda&& callable, TPayload&&... payload)
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	const DelegateHandle handle = m_handleCounter++;
	m_delegates.emplace_back(std::move(DelegateInfo(handle))).delegate.BindLambda(std::forward<TLambda>(callable), std::forward<TPayload>(payload)...);
	return handle;
}

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
void MulticastDelegateBase<isThreadSafe, TReturnType(TArgs...)>::Unbind(DelegateHandle handle)
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	if (m_deferRemovals)
	{
		for (DelegateInfo& delegate : m_delegates)
		{
			if (delegate.handle == handle)
			{
				delegate.delegate.Unbind();
				break;
			}
		}	
	}
	else
	{
		m_delegates.erase(std::remove_if(std::begin(m_delegates), std::end(m_delegates), [ handle ](const DelegateInfo& delegate) { return delegate.handle == handle; }), std::end(m_delegates));
	}
}

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
void MulticastDelegateBase<isThreadSafe, TReturnType(TArgs...)>::Reset()
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	m_delegates.clear();
}

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
Bool MulticastDelegateBase<isThreadSafe, TReturnType(TArgs...)>::IsBound() const
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	return !m_delegates.empty();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Execution =====================================================================================

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
void MulticastDelegateBase<isThreadSafe, TReturnType(TArgs...)>::Broadcast(TArgs... arguments)
{
	Bool foundInvalid = false;

	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();

	const ValueGuard broadcastingGuard(m_deferRemovals, true);

	for (SizeType idx = 0; idx < m_delegates.size(); ++idx)
	{
		if (!m_delegates[idx].delegate.IsBound())
		{
			foundInvalid = true;
			continue;
		}
		m_delegates[idx].delegate.ExecuteIfBound(arguments...);
	}

	if (foundInvalid)
	{
		m_delegates.erase(std::remove_if(std::begin(m_delegates), std::end(m_delegates),
										 [](const DelegateInfo& info)
										 {
											 return !info.delegate.IsBound();
										 }),
						  std::end(m_delegates));
	}
}

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
void MulticastDelegateBase<isThreadSafe, TReturnType(TArgs...)>::ResetAndBroadcast(TArgs... arguments)
{
	lib::DynamicArray<DelegateInfo> localDelegates;

	{
		SPT_MAYBE_UNUSED
		const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
		localDelegates = std::move(m_delegates);
	}

	for (const ThisType::DelegateInfo& delegateInfo : localDelegates)
	{
		delegateInfo.delegate.ExecuteIfBound(arguments...);
	}
}

template<typename... TArgs>
using MulticastDelegate = MulticastDelegateBase<false, TArgs...>;

template<typename... TArgs>
using ThreadSafeMulticastDelegate = MulticastDelegateBase<true, TArgs...>;

}