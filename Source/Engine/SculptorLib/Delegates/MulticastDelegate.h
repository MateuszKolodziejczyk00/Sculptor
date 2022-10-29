#pragma once


#include "SculptorCoreTypes.h"
#include "Delegate.h"
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

	Bool operator==(const DelegateHandle& rhs) const
	{
		return id == rhs.id;
	}

	DelegateIDType id;
};


template<Bool isThreadSafe, typename... TArgs>
class MulticastDelegateBase {};


template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
class MulticastDelegateBase<isThreadSafe, TReturnType(TArgs...)> : private DelegateConditionalThreadSafeData<isThreadSafe>
{
	struct DelegateInfo
	{
		explicit DelegateInfo(DelegateHandle inHandle)
			: handle(inHandle)
		{}

		DelegateBase<isThreadSafe, TReturnType(TArgs...)>	delegate;
		DelegateHandle										handle;
	};

public:

	using ThisType = MulticastDelegateBase<isThreadSafe, TReturnType(TArgs...)>;

	MulticastDelegateBase()
		: m_handleCounter(1)
	{ }

	MulticastDelegateBase(const ThisType& rhs) = delete;
	ThisType& operator=(const ThisType& rhs) = delete;

	MulticastDelegateBase(ThisType&& rhs) = default;
	ThisType& operator=(ThisType&& rhs) = default;

	~MulticastDelegateBase() = default;

	template<typename ObjectType, typename FuncType>
	DelegateHandle		AddMember(ObjectType* user, FuncType function);

	template<typename FuncType>
	DelegateHandle		AddRaw(FuncType* function);

	template<typename Lambda>
	DelegateHandle		AddLambda(Lambda&& functor);

	void				Unbind(DelegateHandle handle);

	void				Reset();

	void				Broadcast(const TArgs&... arguments);

private:

	using ThreadSafeUtils = DelegateConditionalThreadSafeData<isThreadSafe>;

	lib::DynamicArray<DelegateInfo>		m_delegates;
	DelegateIDType						m_handleCounter;
};


template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
template<typename ObjectType, typename FuncType>
DelegateHandle MulticastDelegateBase<isThreadSafe, TReturnType(TArgs...)>::AddMember(ObjectType* user, FuncType function)
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	const DelegateHandle handle = m_handleCounter++;
	m_delegates.emplace_back(std::move(DelegateInfo(handle))).delegate.BindMember(user, function);
	return handle;
}

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
template<typename FuncType>
DelegateHandle MulticastDelegateBase<isThreadSafe, TReturnType(TArgs...)>::AddRaw(FuncType* function)
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	const DelegateHandle handle = m_handleCounter++;
	m_delegates.emplace_back(std::move(DelegateInfo(handle))).delegate.BindRaw(function);
	return handle;
}

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
template<typename Lambda>
DelegateHandle MulticastDelegateBase<isThreadSafe, TReturnType(TArgs...)>::AddLambda(Lambda&& functor)
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	const DelegateHandle handle = m_handleCounter++;
	m_delegates.emplace_back(std::move(DelegateInfo(handle))).delegate.BindLambda(std::forward<Lambda>(functor));
	return handle;
}

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
void MulticastDelegateBase<isThreadSafe, TReturnType(TArgs...)>::Unbind(DelegateHandle handle)
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	std::erase(std::remove_if(m_delegates.begin(), m_delegates.end(), [handle](const DelegateInfo& delegate) { return delegate.Handle == handle; }));
}

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
void MulticastDelegateBase<isThreadSafe, TReturnType(TArgs...)>::Reset()
{
	m_delegates.clear();
}

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
void MulticastDelegateBase<isThreadSafe, TReturnType(TArgs...)>::Broadcast(const TArgs&... arguments)
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	for (const ThisType::DelegateInfo& delegateInfo : m_delegates)
	{
		delegateInfo.delegate.ExecuteIfBound(arguments...);
	}
}

template<typename... TArgs>
using MulticastDelegate = MulticastDelegateBase<false, TArgs...>;

template<typename... TArgs>
using ThreadSafeMulticastDelegate = MulticastDelegateBase<true, TArgs...>;

}