#pragma once


namespace spt::lib::internal
{

template<typename...Args>
class DelegateBindingInterface
{
public:

	virtual ~DelegateBindingInterface() = default;

	virtual Bool IsValid() const = 0;

	virtual void Execute(const Args&... arguments) = 0;
};


template<typename FuncType, typename...Args>
class RawFunctionBinding : public DelegateBindingInterface<Args...>
{
public:

	RawFunctionBinding(FuncType function)
		: m_function(function)
	{
	}

	virtual Bool IsValid() const override
	{
		return m_function != nullptr;
	}

	virtual void Execute(const Args&... arguments) override
	{
		(*m_function)(arguments...);
	}

private:

	FuncType m_function;
};


template<typename UserObject, typename FuncType, typename...Args>
class RawMemberFunctionBinding : public DelegateBindingInterface<Args...>
{
public:

	RawMemberFunctionBinding(UserObject* user, FuncType function)
		: m_user(user)
		, m_function(function)
	{
	}

	virtual Bool IsValid() const override
	{
		return m_user != nullptr && m_function != nullptr;
	}

	virtual void Execute(const Args&... arguments) override
	{
		(m_user->*m_function)(arguments...);
	}

private:

	UserObject* m_user;
	FuncType m_function;
};


template<typename Lambda, typename...Args>
class LambdaBinding : public DelegateBindingInterface<Args...>
{
public:

	LambdaBinding(Lambda&& functor)
		: m_functor(std::forward<Lambda>(functor))
	{
	}

	virtual Bool IsValid() const override
	{
		return true;
	}

	virtual void Execute(const Args&... arguments) override
	{
		m_functor(arguments...);
	}

private:

	Lambda m_functor;
};

} // spt::lib