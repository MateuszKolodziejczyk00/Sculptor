#pragma once


#include "DelegateBindingBuilder.h"
#include "DelegateBindingInstance.h"


namespace spt::lib
{

template<Bool isThreadSafe>
class DelegateConditionalThreadSafeData
{
protected:
	
	using LockType = Bool;

	LockType LockIfNecessary() const
	{
		return LockType();
	}
};


template<>
class DelegateConditionalThreadSafeData<true>
{
protected:

	using LockType = lib::LockGuard<lib::Lock>;

	LockType LockIfNecessary() const
	{
		return LockType(m_lock);
	}

private:

	mutable lib::Lock m_lock;
};

/**
 * Basic Delegate, that can be used for m_binding and executing callables
 */
template<Bool isThreadSafe, typename... Args>
class DelegateBase : private DelegateConditionalThreadSafeData<isThreadSafe>
{
public:

	using ThisType = DelegateBase<isThreadSafe, Args...>;

	DelegateBase() = default;

	DelegateBase(const ThisType& rhs) = delete;
	DelegateBase<isThreadSafe, Args...>& operator=(const ThisType& rhs) = delete;

	DelegateBase(ThisType&& rhs) = default;
	DelegateBase<isThreadSafe, Args...>& operator=(ThisType&& rhs) = default;

	~DelegateBase() = default;

	template<typename ObjectType, typename FuncType>
	void BindMember(ObjectType* user, FuncType function);

	template<typename FuncType>
	void BindRaw(FuncType* function);

	template<typename Lambda>
	void BindLambda(Lambda&& functor);

	void Unbind();

	/** returns true, if currently any object is bound, and it's valid */
	Bool IsBound() const;

	/** Invokes currently bound function if it's valid */
	void ExecuteIfBound(const Args&... arguments) const;

private:

	using ThreadSafeUtils = DelegateConditionalThreadSafeData<isThreadSafe>;

	UniquePtr<internal::DelegateBindingInterface<Args...>> m_binding;
};

template<Bool isThreadSafe, typename... Args>
template<typename ObjectType, typename FuncType>
void DelegateBase<isThreadSafe, Args...>::BindMember(ObjectType* user, FuncType function)
{
	[[maybe_unused]]
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	m_binding = internal::DelegateBindingBuilder::CreateMemberBinding<ObjectType, FuncType, Args...>(user, function);
}

template<Bool isThreadSafe, typename... Args>
template<typename FuncType>
void DelegateBase<isThreadSafe, Args...>::BindRaw(FuncType* function)
{
	[[maybe_unused]]
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	m_binding = internal::DelegateBindingBuilder::CreateBinding<FuncType, Args...>(function);
}

template<Bool isThreadSafe, typename... Args>
template<typename Lambda>
void DelegateBase<isThreadSafe, Args...>::BindLambda(Lambda&& functor)
{
	[[maybe_unused]]
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	m_binding = internal::DelegateBindingBuilder::CreateLambda<Lambda, Args...>(std::forward<Lambda>(functor));
}

template<Bool isThreadSafe, typename... Args>
void DelegateBase<isThreadSafe, Args...>::ExecuteIfBound(const Args&... arguments) const
{
	[[maybe_unused]]
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	if (m_binding.get() && m_binding->IsValid())
	{
		m_binding->Execute(arguments...);
	}
}

template<Bool isThreadSafe, typename... Args>
void DelegateBase<isThreadSafe, Args...>::Unbind()
{
	[[maybe_unused]]
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	m_binding.reset();
}

template<Bool isThreadSafe, typename... Args>
Bool DelegateBase<isThreadSafe, Args...>::IsBound() const
{
	return m_binding.get() && m_binding->IsValid();
}

template<typename... Args>
using Delegate = DelegateBase<false, Args...>;

template<typename... Args>
using ThreadSafeDelegate = DelegateBase<true, Args...>;


}