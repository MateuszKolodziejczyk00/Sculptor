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

	template<typename FuncType, typename... TArgs>
	static UniquePtr<DelegateBindingInterface<TArgs...>> CreateBinding(FuncType* function);

	template<typename UserObject, typename FuncType, typename... TArgs>
	static UniquePtr<DelegateBindingInterface<TArgs...>> CreateMemberBinding(UserObject* user, FuncType function);

	template<typename Lambda, typename... TArgs>
	static UniquePtr<DelegateBindingInterface<TArgs...>> CreateLambda(Lambda&& functor);
};


template<typename FuncType, typename... TArgs>
UniquePtr<DelegateBindingInterface<TArgs...>> DelegateBindingBuilder::CreateBinding(FuncType* function)
{
	return UniquePtr<DelegateBindingInterface<TArgs...>>(new RawFunctionBinding<FuncType, TArgs...>(function));
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
