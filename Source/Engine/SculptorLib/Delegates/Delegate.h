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

	SPT_NODISCARD LockType LockIfNecessary() const
	{
		return LockType(m_lock);
	}

private:

	mutable lib::Lock m_lock;
};


template<Bool isThreadSafe, typename... TArgs>
class DelegateBase {};


/**
 * Basic Delegate, that can be used for m_binding and executing callables
 */
template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
class DelegateBase<isThreadSafe, TReturnType(TArgs...)> : private DelegateConditionalThreadSafeData<isThreadSafe>
{
public:

	using ThisType = DelegateBase<isThreadSafe, TReturnType(TArgs...)>;

	DelegateBase() = default;

	DelegateBase(const ThisType& rhs) = delete;
	DelegateBase& operator=(const ThisType& rhs) = delete;

	DelegateBase(ThisType&& rhs) = default;
	DelegateBase& operator=(ThisType&& rhs) = default;

	~DelegateBase() = default;

	template<typename ObjectType, typename FuncType>
	void BindMember(ObjectType* user, FuncType function);

	template<typename TFuncType, typename... TPayload>
	void BindRaw(TFuncType function, TPayload&&... payload);

	template<typename Lambda>
	void BindLambda(Lambda&& functor);

	void Unbind();

	/** returns true, if currently any object is bound, and it's valid */
	Bool IsBound() const;

	/** Invokes currently bound function if it's valid */
	void ExecuteIfBound(const TArgs&... arguments) const;

private:

	using ThreadSafeUtils = DelegateConditionalThreadSafeData<isThreadSafe>;

	UniquePtr<internal::DelegateBindingInterface<TReturnType(TArgs...)>> m_binding;
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// Binds =========================================================================================

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
template<typename ObjectType, typename FuncType>
void DelegateBase<isThreadSafe, TReturnType(TArgs...)>::BindMember(ObjectType* user, FuncType function)
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	m_binding = internal::DelegateBindingBuilder::CreateMemberBinding<ObjectType, FuncType, TReturnType(TArgs...)>(user, function);
}

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
template<typename TFuncType, typename... TPayload>
void DelegateBase<isThreadSafe, TReturnType(TArgs...)>::BindRaw(TFuncType function, TPayload&&... payload)
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	m_binding = internal::DelegateBindingBuilder::CreateRawBinding<TFuncType, TReturnType(TArgs...), TPayload...>(function, std::forward<TPayload>(payload)...);
}

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
template<typename Lambda>
void DelegateBase<isThreadSafe, TReturnType(TArgs...)>::BindLambda(Lambda&& functor)
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	m_binding = internal::DelegateBindingBuilder::CreateLambda<Lambda, TReturnType(TArgs...)>(std::forward<Lambda>(functor));
}

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
void DelegateBase<isThreadSafe, TReturnType(TArgs...)>::Unbind()
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	m_binding.reset();
}

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
Bool DelegateBase<isThreadSafe, TReturnType(TArgs...)>::IsBound() const
{
	return m_binding.get() && m_binding->IsValid();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Execution =====================================================================================

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
void DelegateBase<isThreadSafe, TReturnType(TArgs...)>::ExecuteIfBound(const TArgs&... arguments) const
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	if (m_binding.get() && m_binding->IsValid())
	{
		m_binding->Execute(arguments...);
	}
}

template<typename... TArgs>
using Delegate = DelegateBase<false, TArgs...>;

template<typename... TArgs>
using ThreadSafeDelegate = DelegateBase<true, TArgs...>;

} // spt::lib