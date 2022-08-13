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
		: m_Id(-1)
	{ }

	DelegateHandle(Int32 id)
		: m_Id(id)
	{ }

	Bool IsValid() const
	{
		return m_Id != -1;
	}

	Bool operator==(const DelegateHandle& rhs) const
	{
		return m_Id == rhs.m_Id;
	}

	DelegateIDType m_Id;
};


template<Bool isThreadSafe, typename... Args>
class MulticastDelegateBase : private DelegateConditionalThreadSafeData<isThreadSafe>
{
	struct DelegateInfo
	{
		explicit DelegateInfo(DelegateHandle handle)
			: m_Handle(handle)
		{}

		Delegate<Args...> m_Delegate;
		DelegateHandle m_Handle;
	};

public:

	using ThisType = MulticastDelegateBase<isThreadSafe, Args...>;

	MulticastDelegateBase() = default;

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

	void				Broadcast(const Args&... arguments);

private:

	using ThreadSafeUtils = DelegateConditionalThreadSafeData<isThreadSafe>;

	lib::DynamicArray<DelegateInfo>		m_delegates;
	DelegateIDType						m_handleCounter;
};


template<Bool isThreadSafe, typename... Args>
template<typename ObjectType, typename FuncType>
DelegateHandle MulticastDelegateBase<isThreadSafe, Args...>::AddMember(ObjectType* user, FuncType function)
{
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	const DelegateHandle handle = m_handleCounter++;
	m_delegates.emplace_back(std::move(DelegateInfo(handle))).m_Delegate.BindMember(user, function);
	return handle;
}

template<Bool isThreadSafe, typename... Args>
template<typename FuncType>
DelegateHandle MulticastDelegateBase<isThreadSafe, Args...>::AddRaw(FuncType* function)
{
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	const DelegateHandle handle = m_handleCounter++;
	m_delegates.emplace_back(std::move(DelegateInfo(handle))).m_Delegate.BindRaw(function);
	return handle;
}

template<Bool isThreadSafe, typename... Args>
template<typename Lambda>
DelegateHandle MulticastDelegateBase<isThreadSafe, Args...>::AddLambda(Lambda&& functor)
{
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	const DelegateHandle handle = m_handleCounter++;
	m_delegates.emplace_back(std::move(DelegateInfo(handle))).m_Delegate.BindLambda(std::forward<Lambda>(functor));
	return handle;
}

template<Bool isThreadSafe, typename... Args>
void MulticastDelegateBase<isThreadSafe, Args...>::Unbind(DelegateHandle handle)
{
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	std::erase(std::remove_if(m_delegates.begin(), m_delegates.end(), [handle](const DelegateInfo& delegate) { return delegate.m_Handle == handle; }));
}

template<Bool isThreadSafe, typename... Args>
void MulticastDelegateBase<isThreadSafe, Args...>::Reset()
{
	m_delegates.clear();
}

template<Bool isThreadSafe, typename... Args>
void MulticastDelegateBase<isThreadSafe, Args...>::Broadcast(const Args&... arguments)
{
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	for (const ThisType::DelegateInfo& delegateInfo : m_delegates)
	{
		delegateInfo.m_Delegate.ExecuteIfBound(arguments...);
	}
}

template<typename... Args>
using MulticastDelegate = MulticastDelegateBase<false, Args...>;

template<typename... Args>
using ThreadSafeMulticastDelegate = MulticastDelegateBase<true, Args...>;

}