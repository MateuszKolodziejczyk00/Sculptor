#pragma once

#include "DelegateBindingInstance.h"


namespace spt::lib::internal
{

/**
 * Builder used for selecting proper binding and creating it with proper settings
 */
class DelegateBindingBuilder
{
public:

	template<typename TFuncType, typename TDelegateSignature, typename... TPayload>
	static UniquePtr<DelegateBindingInterface<TDelegateSignature>> CreateRawBinding(TFuncType function, TPayload&&... payload);

	template<typename UserObject, typename FuncType, typename... TArgs>
	static UniquePtr<DelegateBindingInterface<TArgs...>> CreateMemberBinding(UserObject* user, FuncType function);

	template<typename Lambda, typename... TArgs>
	static UniquePtr<DelegateBindingInterface<TArgs...>> CreateLambda(Lambda&& functor);
};


template<typename TFuncType, typename TDelegateSignature, typename... TPayload>
UniquePtr<DelegateBindingInterface<TDelegateSignature>> DelegateBindingBuilder::CreateRawBinding(TFuncType function, TPayload&&... payload)
{
	return std::make_unique<RawFunctionBinding<TFuncType, TDelegateSignature, TPayload...>>(function, std::forward<TPayload>(payload)...);
}

template<typename UserObject, typename FuncType, typename... TArgs>
UniquePtr<DelegateBindingInterface<TArgs...>> DelegateBindingBuilder::CreateMemberBinding(UserObject* user, FuncType function)
{
	return UniquePtr<DelegateBindingInterface<TArgs...>>(new RawMemberFunctionBinding<UserObject, FuncType, TArgs...>(user, function));
}

template<typename Lambda, typename... TArgs>
UniquePtr<DelegateBindingInterface<TArgs...>> DelegateBindingBuilder::CreateLambda(Lambda&& functor)
{
	return UniquePtr<DelegateBindingInterface<TArgs...>>(new LambdaBinding<Lambda, TArgs...>(std::forward<Lambda>(functor)));
}

} // spt::lib::internal
