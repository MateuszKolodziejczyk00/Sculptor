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

	template<typename TFuncType, typename... TPayload>
	void BindRaw(TFuncType function, TPayload&&... payload);

	template<typename TObjectType, typename TFuncType, typename... TPayload>
	void BindRawMember(TObjectType* object, TFuncType function, TPayload&&... payload);

	template<typename TObjectType, typename TFuncType, typename... TPayload>
	void BindSharedMember(lib::SharedPtr<TObjectType> object, TFuncType function, TPayload&&... payload);

	template<typename TObjectType, typename TFuncType, typename... TPayload>
	void BindWeakMember(const lib::SharedPtr<TObjectType>& object, TFuncType function, TPayload&&... payload);

	template<typename TLambda, typename... TPayload>
	void BindLambda(TLambda&& callable, TPayload&&... payload);

	void Unbind();

	/** returns true, if currently any object is bound, and it's valid */
	Bool IsBound() const;

	/** Invokes currently bound function if it's valid */
	void ExecuteIfBound(TArgs... arguments) const;

private:

	using ThreadSafeUtils = DelegateConditionalThreadSafeData<isThreadSafe>;

	UniquePtr<internal::DelegateBindingInterface<TReturnType(TArgs...)>> m_binding;
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// Binds =========================================================================================

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
template<typename TFuncType, typename... TPayload>
void DelegateBase<isThreadSafe, TReturnType(TArgs...)>::BindRaw(TFuncType function, TPayload&&... payload)
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	m_binding = internal::DelegateBindingBuilder::CreateRawBinding<TFuncType, TReturnType(TArgs...), TPayload...>(function, std::forward<TPayload>(payload)...);
}

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
template<typename TObjectType, typename TFuncType, typename... TPayload>
void DelegateBase<isThreadSafe, TReturnType(TArgs...)>::BindRawMember(TObjectType* object, TFuncType function, TPayload&&... payload)
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	m_binding = internal::DelegateBindingBuilder::CreateRawMemberBinding<TObjectType, TFuncType, TReturnType(TArgs...), TPayload...>(object, function, std::forward<TPayload>(payload)...);
}

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
template<typename TObjectType, typename TFuncType, typename... TPayload>
void DelegateBase<isThreadSafe, TReturnType(TArgs...)>::BindSharedMember(lib::SharedPtr<TObjectType> object, TFuncType function, TPayload&&... payload)
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	m_binding = internal::DelegateBindingBuilder::CreateSharedMemberBinding<TObjectType, TFuncType, TReturnType(TArgs...), TPayload...>(std::move(object), function, std::forward<TPayload>(payload)...);
}

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
template<typename TObjectType, typename TFuncType, typename... TPayload>
void DelegateBase<isThreadSafe, TReturnType(TArgs...)>::BindWeakMember(const lib::SharedPtr<TObjectType>& object, TFuncType function, TPayload&&... payload)
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	m_binding = internal::DelegateBindingBuilder::CreateWeakMemberBinding<TObjectType, TFuncType, TReturnType(TArgs...), TPayload...>(object, function, std::forward<TPayload>(payload)...);
}

template<Bool isThreadSafe, typename TReturnType, typename... TArgs>
template<typename TLambda, typename... TPayload>
void DelegateBase<isThreadSafe, TReturnType(TArgs...)>::BindLambda(TLambda&& callable, TPayload&&... payload)
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	m_binding = internal::DelegateBindingBuilder::CreateLambda<TLambda, TReturnType(TArgs...), TPayload...>(std::forward<TLambda>(callable), std::forward<TPayload>(payload)...);
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
void DelegateBase<isThreadSafe, TReturnType(TArgs...)>::ExecuteIfBound(TArgs... arguments) const
{
	SPT_MAYBE_UNUSED
	const typename ThreadSafeUtils::LockType lock = ThreadSafeUtils::LockIfNecessary();
	if (m_binding.get() && m_binding->IsValid())
	{
		m_binding->Execute(std::forward<TArgs>(arguments)...);
	}
}

template<typename... TArgs>
using Delegate = DelegateBase<false, TArgs...>;

template<typename... TArgs>
using ThreadSafeDelegate = DelegateBase<true, TArgs...>;

} // spt::lib