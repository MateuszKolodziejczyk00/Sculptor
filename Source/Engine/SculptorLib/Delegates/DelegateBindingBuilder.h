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

	template<typename FuncType, typename...Args>
	static UniquePtr<DelegateBindingInterface<Args...>> CreateBinding(FuncType* function);

	template<typename UserObject, typename FuncType, typename...Args>
	static UniquePtr<DelegateBindingInterface<Args...>> CreateMemberBinding(UserObject* user, FuncType function);

	template<typename Lambda, typename...Args>
	static UniquePtr<DelegateBindingInterface<Args...>> CreateLambda(Lambda&& functor);
};


template<typename FuncType, typename...Args>
static UniquePtr<DelegateBindingInterface<Args...>> DelegateBindingBuilder::CreateBinding(FuncType* function)
{
	return UniquePtr<DelegateBindingInterface<Args...>>(new RawFunctionBinding<FuncType, Args...>(function));
}

template<typename UserObject, typename FuncType, typename...Args>
static UniquePtr<DelegateBindingInterface<Args...>> DelegateBindingBuilder::CreateMemberBinding(UserObject* user, FuncType function)
{
	return UniquePtr<DelegateBindingInterface<Args...>>(new RawMemberFunctionBinding<UserObject, FuncType, Args...>(user, function));
}

template<typename Lambda, typename...Args>
static UniquePtr<DelegateBindingInterface<Args...>> DelegateBindingBuilder::CreateLambda(Lambda&& functor)
{
	return UniquePtr<DelegateBindingInterface<Args...>>(new LambdaBinding<Lambda, Args...>(std::forward<Lambda>(functor)));
}

}
