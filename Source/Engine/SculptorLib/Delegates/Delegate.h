#pragma once


#include "DelegateBindingBuilder.h"
#include "DelegateBindingInstance.h"


namespace spt::lib
{

/**
 * Basic delegate, that can be used for m_binding and executing callables
 */
template<typename... Args>
class Delegate
{
public:

	Delegate() = default;

	Delegate(const Delegate<Args...>& rhs) = delete;
	Delegate<Args...>& operator=(const Delegate<Args...>& rhs) = delete;

	Delegate(Delegate<Args...>&& rhs) = default;
	Delegate<Args...>& operator=(Delegate<Args...>&& rhs) = default;

	~Delegate() = default;

	template<typename ObjectType, typename FuncType>
	void BindMember(ObjectType* user, FuncType function);

	template<typename FuncType>
	void BindRaw(FuncType* function);

	template<typename Lambda>
	void BindLambda(const Lambda& functor);

	void Unbind();

	/** returns true, if currently any object is bound, and it's valid */
	bool IsBound() const;

	/** Invokes currently bound function if it's valid */
	void ExecuteIfBound(const Args&... arguments) const;

private:

	std::unique_ptr<internal::DelegateBindingInterface<Args...>> m_binding;
};

template<typename... Args>
template<typename ObjectType, typename FuncType>
void Delegate<Args...>::BindMember(ObjectType* user, FuncType function)
{
	m_binding = internal::DelegateBindingBuilder::CreateMemberBinding<ObjectType, FuncType, Args...>(user, function);
}

template<typename... Args>
template<typename FuncType>
void Delegate<Args...>::BindRaw(FuncType* function)
{
	m_binding = internal::DelegateBindingBuilder::CreateBinding<FuncType, Args...>(function);
}

template<typename... Args>
template<typename Lambda>
void Delegate<Args...>::BindLambda(const Lambda& functor)
{
	m_binding = internal::DelegateBindingBuilder::CreateLambda<Lambda, Args...>(functor);
}

template<typename... Args>
void Delegate<Args...>::ExecuteIfBound(const Args&... arguments) const
{
	if (m_binding.get() && m_binding->IsValid())
	{
		m_binding->Execute(arguments...);
	}
}

template<typename... Args>
void Delegate<Args...>::Unbind()
{
	m_binding.reset();
}

template<typename... Args>
bool Delegate<Args...>::IsBound() const
{
	return m_binding.get() && m_binding->IsValid();
}

}