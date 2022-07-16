#pragma once


#include "DelegateBindingInstance.h"
#include <memory>


namespace spt::lib::internal
{

/**
 * Builder used for selecting proper binding and creating it with proper settings
 */
class DelegateBindingBuilder
{
public:

	template<typename FuncType, typename...Args>
	static std::unique_ptr<DelegateBindingInterface<Args...>> CreateBinding(FuncType* function);

	template<typename UserObject, typename FuncType, typename...Args>
	static std::unique_ptr<DelegateBindingInterface<Args...>> CreateMemberBinding(UserObject* user, FuncType function);

	template<typename Lambda, typename...Args>
	static std::unique_ptr<DelegateBindingInterface<Args...>> CreateLambda(const Lambda& functor);
};


template<typename FuncType, typename...Args>
static std::unique_ptr<DelegateBindingInterface<Args...>> DelegateBindingBuilder::CreateBinding(FuncType* function)
{
	return std::unique_ptr<DelegateBindingInterface<Args...>>(new RawFunctionBinding<FuncType, Args...>(function));
}

template<typename UserObject, typename FuncType, typename...Args>
static std::unique_ptr<DelegateBindingInterface<Args...>> DelegateBindingBuilder::CreateMemberBinding(UserObject* user, FuncType function)
{
	return std::unique_ptr<DelegateBindingInterface<Args...>>(new RawMemberFunctionBinding<UserObject, FuncType, Args...>(user, function));
}

template<typename Lambda, typename...Args>
static std::unique_ptr<DelegateBindingInterface<Args...>> DelegateBindingBuilder::CreateLambda(const Lambda& functor)
{
	return std::unique_ptr<DelegateBindingInterface<Args...>>(new LambdaBinding<Lambda, Args...>(functor));
}

}
