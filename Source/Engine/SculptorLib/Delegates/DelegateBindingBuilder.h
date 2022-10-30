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

	template<typename TObjectType, typename TFuncType, typename TDelegateSignature, typename... TPayload>
	static UniquePtr<DelegateBindingInterface<TDelegateSignature>> CreateRawMemberBinding(TObjectType* object, TFuncType function, TPayload&&... payload);

	template<typename TLambda, typename TDelegateSignature, typename... TPayload>
	static UniquePtr<DelegateBindingInterface<TDelegateSignature>> CreateLambda(TLambda&& callable, TPayload&&... payload);
};


template<typename TFuncType, typename TDelegateSignature, typename... TPayload>
UniquePtr<DelegateBindingInterface<TDelegateSignature>> DelegateBindingBuilder::CreateRawBinding(TFuncType function, TPayload&&... payload)
{
	return std::make_unique<RawFunctionBinding<TFuncType, TDelegateSignature, TPayload...>>(function, std::forward<TPayload>(payload)...);
}

template<typename TObjectType, typename TFuncType, typename TDelegateSignature, typename... TPayload>
UniquePtr<DelegateBindingInterface<TDelegateSignature>> DelegateBindingBuilder::CreateRawMemberBinding(TObjectType* object, TFuncType function, TPayload&&... payload)
{
	return std::make_unique<RawMemberFunctionBinding<TObjectType, TFuncType, TDelegateSignature, TPayload...>>(object, function, std::forward<TPayload>(payload)...);
}

template<typename TLambda, typename TDelegateSignature, typename... TPayload>
UniquePtr<DelegateBindingInterface<TDelegateSignature>> DelegateBindingBuilder::CreateLambda(TLambda&& callable, TPayload&&... payload)
{
	return std::make_unique<LambdaBinding<TLambda, TDelegateSignature, TPayload...>>(std::forward<TLambda>(callable), std::forward<TPayload>(payload)...);
}

} // spt::lib::internal
