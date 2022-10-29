#pragma once


namespace spt::lib::internal
{

template<typename... TArgs>
class DelegateBindingInterface {};

template<typename TReturnType, typename... TArgs>
class DelegateBindingInterface<TReturnType(TArgs...)>
{
public:

	DelegateBindingInterface() = default;
	virtual ~DelegateBindingInterface() = default;

	virtual Bool IsValid() const = 0;

	virtual TReturnType Execute(TArgs... arguments) = 0;
};

template<typename TFuncType, typename... TArgs>
class RawFunctionBinding {};

template<typename TFuncType, typename TReturnType, typename... TArgs>
class RawFunctionBinding<TFuncType, TReturnType(TArgs...)> : public DelegateBindingInterface<TReturnType(TArgs...)>
{
public:

	explicit RawFunctionBinding(TFuncType function)
		: m_function(function)
	{ }

	virtual Bool IsValid() const override
	{
		return m_function != nullptr;
	}

	virtual TReturnType Execute(TArgs... arguments) override
	{
		return (*m_function)(arguments...);
	}

private:

	TFuncType m_function;
};


template<typename TObjectType, typename TFuncType, typename... TArgs>
class RawMemberFunctionBinding {};

template<typename TObjectType, typename TFuncType, typename TReturnType, typename... TArgs>
class RawMemberFunctionBinding<TObjectType, TFuncType, TReturnType(TArgs...)> : public DelegateBindingInterface<TReturnType(TArgs...)>
{
public:

	RawMemberFunctionBinding(TObjectType* object, TFuncType function)
		: m_object(object)
		, m_function(function)
	{
	}

	virtual Bool IsValid() const override
	{
		return m_object != nullptr && m_function != nullptr;
	}

	virtual TReturnType Execute(const TArgs&... arguments) override
	{
		return (m_object->*m_function)(arguments...);
	}

private:

	TObjectType*	m_object;
	TFuncType		m_function;
};


template<typename TObjectType, typename TFuncType, typename... TArgs>
class SharedMemberFunctionBinding {};

template<typename TObjectType, typename TFuncType, typename TReturnType, typename... TArgs>
class SharedMemberFunctionBinding<TObjectType, TFuncType, TReturnType(TArgs...)> : public DelegateBindingInterface<TReturnType(TArgs...)>
{
public:

	SharedMemberFunctionBinding(lib::SharedPtr<TObjectType> object, TFuncType function)
		: m_object(object)
		, m_function(function)
	{ }

	virtual Bool IsValid() const override
	{
		return m_object && m_function != nullptr;
	}

	virtual TReturnType Execute(TArgs... arguments) override
	{
		return (m_object->*m_function)(arguments...);
	}

private:

	lib::SharedPtr<TObjectType> m_object;
	TFuncType					m_function;
};


template<typename TCallable, typename... TArgs>
class LambdaBinding {};

template<typename TCallable, typename TReturnType, typename... TArgs>
class LambdaBinding<TCallable, TReturnType(TArgs...)> : public DelegateBindingInterface<TReturnType(TArgs...)>
{
public:

	explicit LambdaBinding(TCallable&& callable)
		: m_callable(std::forward<TCallable>(callable))
	{ }

	virtual Bool IsValid() const override
	{
		return true;
	}

	virtual TReturnType Execute(TArgs... arguments) override
	{
		return m_callable(std::forward<TArgs>(arguments)...);
	}

private:

	TCallable m_callable;
};

} // spt::lib