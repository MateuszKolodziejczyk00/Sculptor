#pragma once


#include "SculptorCoreTypes.h"
#include "Delegate.h"
#include <vector>
#include <mutex>


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


template<Bool isThreadsafe, typename... Args>
class MulticastDelegateBase
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

	using ThisType = MulticastDelegateBase<isThreadsafe, Args...>;

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

	using MutexOrBool					= std::conditional_t<isThreadsafe, std::mutex, Bool>;

	lib::DynamicArray<DelegateInfo>		m_delegates;
	DelegateIDType						m_handleCounter;
	MutexOrBool							m_lock;
};


template<Bool isThreadsafe, typename... Args>
template<typename ObjectType, typename FuncType>
DelegateHandle MulticastDelegateBase<isThreadsafe, Args...>::AddMember(ObjectType* user, FuncType function)
{
	if constexpr (isThreadsafe)
	{
		m_lock.lock();
	}

	const DelegateHandle handle = m_handleCounter++;
	m_delegates.emplace_back(std::move(DelegateInfo(handle))).m_Delegate.BindMember(user, function);

	if constexpr (isThreadsafe)
	{
		m_lock.unlock();
	}

	return handle;
}

template<Bool isThreadsafe, typename... Args>
template<typename FuncType>
DelegateHandle MulticastDelegateBase<isThreadsafe, Args...>::AddRaw(FuncType* function)
{
	if constexpr (isThreadsafe)
	{
		m_lock.lock();
	}

	const DelegateHandle handle = m_handleCounter++;
	m_delegates.emplace_back(std::move(DelegateInfo(handle))).m_Delegate.BindRaw(function);
	
	if constexpr (isThreadsafe)
	{
		m_lock.unlock();
	}

	return handle;
}

template<Bool isThreadsafe, typename... Args>
template<typename Lambda>
DelegateHandle MulticastDelegateBase<isThreadsafe, Args...>::AddLambda(Lambda&& functor)
{
	if constexpr (isThreadsafe)
	{
		m_lock.lock();
	}

	const DelegateHandle handle = m_handleCounter++;
	m_delegates.emplace_back(std::move(DelegateInfo(handle))).m_Delegate.BindLambda(std::forward<Lambda>(functor));

	if constexpr (isThreadsafe)
	{
		m_lock.unlock();
	}

	return handle;
}

template<Bool isThreadsafe, typename... Args>
void MulticastDelegateBase<isThreadsafe, Args...>::Unbind(DelegateHandle handle)
{
	if constexpr (isThreadsafe)
	{
		m_lock.lock();
	}

	std::erase(std::remove_if(m_delegates.begin(), m_delegates.end(), [handle](const DelegateInfo& delegate) { return delegate.m_Handle == handle; }));

	if constexpr (isThreadsafe)
	{
		m_lock.unlock();
	}
}

template<Bool isThreadsafe, typename... Args>
void MulticastDelegateBase<isThreadsafe, Args...>::Reset()
{
	m_delegates.clear();
}

template<Bool isThreadsafe, typename... Args>
void MulticastDelegateBase<isThreadsafe, Args...>::Broadcast(const Args&... arguments)
{
	if constexpr (isThreadsafe)
	{
		m_lock.lock();
	}

	for (const ThisType::DelegateInfo& delegateInfo : m_delegates)
	{
		delegateInfo.m_Delegate.ExecuteIfBound(arguments...);
	}

	if constexpr (isThreadsafe)
	{
		m_lock.unlock();
	}
}

template<typename... Args>
using MulticastDelegate = MulticastDelegateBase<false, Args...>;

template<typename... Args>
using ThreadsafeMulticastDelegate = MulticastDelegateBase<true, Args...>;

}