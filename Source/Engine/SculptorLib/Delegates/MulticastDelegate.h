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


template<typename... Args>
class MulticastDelegate
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

	MulticastDelegate() = default;

	MulticastDelegate(const MulticastDelegate<Args...>& rhs) = delete;
	MulticastDelegate<Args...>& operator=(const MulticastDelegate<Args...>& rhs) = delete;

	MulticastDelegate(MulticastDelegate<Args...>&& rhs) = default;
	MulticastDelegate<Args...>& operator=(MulticastDelegate<Args...>&& rhs) = default;

	~MulticastDelegate() = default;

	template<typename ObjectType, typename FuncType>
	DelegateHandle		AddMember(ObjectType* user, FuncType function);

	template<typename FuncType>
	DelegateHandle		AddRaw(FuncType* function);

	template<typename Lambda>
	DelegateHandle		AddLambda(const Lambda& functor);

	void				Unbind(DelegateHandle handle);

	void				Reset();

	void				Broadcast(const Args&... arguments);

private:

	lib::DynamicArray<DelegateInfo> m_delegates;
	DelegateIDType m_handleCounter;
};


template<typename... Args>
template<typename ObjectType, typename FuncType>
DelegateHandle MulticastDelegate<Args...>::AddMember(ObjectType* user, FuncType function)
{
	const DelegateHandle handle = m_handleCounter++;
	m_delegates.emplace_back(std::move(DelegateInfo(handle))).m_Delegate.BindMember(user, function);
	return handle;
}

template<typename... Args>
template<typename FuncType>
DelegateHandle MulticastDelegate<Args...>::AddRaw(FuncType* function)
{
	const DelegateHandle handle = m_handleCounter++;
	m_delegates.emplace_back(std::move(DelegateInfo(handle))).m_Delegate.BindRaw(function);
	return handle;
}

template<typename... Args>
template<typename Lambda>
DelegateHandle MulticastDelegate<Args...>::AddLambda(const Lambda& functor)
{
	const DelegateHandle handle = m_handleCounter++;
	m_delegates.emplace_back(std::move(DelegateInfo(handle))).m_Delegate.BindLambda(functor);
	return handle;
}

template<typename... Args>
void MulticastDelegate<Args...>::Unbind(DelegateHandle handle)
{
	std::erase(std::remove_if(m_delegates.begin(), m_delegates.end(), [handle](const DelegateInfo& delegate) { return delegate.m_Handle == handle; }));
}

template<typename... Args>
void MulticastDelegate<Args...>::Reset()
{
	m_delegates.clear();
}

template<typename... Args>
void MulticastDelegate<Args...>::Broadcast(const Args&... arguments)
{
	for (const MulticastDelegate<Args...>::DelegateInfo& delegateInfo : m_delegates)
	{
		delegateInfo.m_Delegate.ExecuteIfBound(arguments...);
	}
}

}