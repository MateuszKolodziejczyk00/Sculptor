#pragma once

#include <tuple>


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

	virtual TReturnType Execute(TArgs&&... arguments) = 0;
};


template<typename... TArgs>
class DelegateBindingWithPayload {};

template<typename TReturnType, typename... TArgs, typename... TPayload>
class DelegateBindingWithPayload<TReturnType(TArgs...), TPayload...> : public DelegateBindingInterface<TReturnType(TArgs...)>
{
protected:

	using Super = DelegateBindingInterface<TReturnType(TArgs...)>;

public:
	
	using PayloadType = std::tuple<TPayload...>;

	template<typename... TInputPayload>
	explicit DelegateBindingWithPayload(TInputPayload&&... payload)
		: m_payload(std::forward<TInputPayload>(payload)...)
	{ }

protected:

	PayloadType& GetPayload()
	{
		return m_payload;
	}

private:

	PayloadType m_payload;
};


template<typename TFuncType, typename... TArgs>
class RawFunctionBinding {};

template<typename TFuncType, typename TReturnType, typename... TArgs, typename... TPayload>
class RawFunctionBinding<TFuncType, TReturnType(TArgs...), TPayload...> : public DelegateBindingWithPayload<TReturnType(TArgs...), TPayload...>
{
protected:

	using Super = DelegateBindingWithPayload<TReturnType(TArgs...), TPayload...>;

public:

	template<typename... TInputPayload>
	explicit RawFunctionBinding(TFuncType function, TInputPayload&&... payload)
		: Super(std::forward<TInputPayload>(payload)...)
		, m_function(function)
	{ }

	virtual Bool IsValid() const override
	{
		return m_function != nullptr;
	}

	virtual TReturnType Execute(TArgs&&... arguments) override
	{
		return ExecuteImpl(std::forward<TArgs>(arguments)..., std::index_sequence_for<TPayload...>{});
	}

private:

	template<SizeType... indices>
	TReturnType ExecuteImpl(TArgs&&... arguments, std::index_sequence<indices...>)
	{
		return (*m_function)(std::forward<TArgs>(arguments)..., std::get<indices>(Super::GetPayload())...);
	}

	TFuncType m_function;
};


template<typename TObjectType, typename TFuncType, typename... TArgs>
class RawMemberFunctionBinding {};

template<typename TObjectType, typename TFuncType, typename TReturnType, typename... TArgs, typename... TPayload>
class RawMemberFunctionBinding<TObjectType, TFuncType, TReturnType(TArgs...), TPayload...> : public DelegateBindingWithPayload<TReturnType(TArgs...), TPayload...>
{
protected:

	using Super = DelegateBindingWithPayload<TReturnType(TArgs...), TPayload...>;

public:

	template<typename... TInputPayload>
	explicit RawMemberFunctionBinding(TObjectType* object, TFuncType function, TInputPayload&&... payload)
		: Super(std::forward<TInputPayload>(payload)...)
		, m_object(object)
		, m_function(function)
	{
	}

	virtual Bool IsValid() const override
	{
		return m_object != nullptr && m_function != nullptr;
	}

	virtual TReturnType Execute(TArgs&&... arguments) override
	{
		return ExecuteImpl(std::forward<TArgs>(arguments)..., std::index_sequence_for<TPayload...>{});
	}

private:

	template<SizeType... indices>
	TReturnType ExecuteImpl(TArgs&&... arguments, std::index_sequence<indices...>)
	{
		return (m_object->*m_function)(std::forward<TArgs>(arguments)..., std::get<indices>(Super::GetPayload())...);
	}

	TObjectType*	m_object;
	TFuncType		m_function;
};


template<typename TObjectType, typename TFuncType, typename... TArgs>
class SharedMemberFunctionBinding {};

template<typename TObjectType, typename TFuncType, typename TReturnType, typename... TArgs, typename... TPayload>
class SharedMemberFunctionBinding<TObjectType, TFuncType, TReturnType(TArgs...), TPayload...> : public DelegateBindingWithPayload<TReturnType(TArgs...), TPayload...>
{
protected:

	using Super = DelegateBindingWithPayload<TReturnType(TArgs...), TPayload...>;

public:

	template<typename... TInputPayload>
	SharedMemberFunctionBinding(lib::SharedPtr<TObjectType> object, TFuncType function, TInputPayload&&... payload)
		: Super(std::forward<TInputPayload>(payload)...)
		, m_object(std::move(object))
		, m_function(function)
	{ }

	virtual Bool IsValid() const override
	{
		return m_object && m_function != nullptr;
	}

	virtual TReturnType Execute(TArgs&&... arguments) override
	{
		return ExecuteImpl(std::forward<TArgs>(arguments)..., std::index_sequence_for<TPayload...>{});
	}

private:

	template<SizeType... indices>
	TReturnType ExecuteImpl(TArgs&&... arguments, std::index_sequence<indices...>)
	{
		return (m_object.get()->*m_function)(std::forward<TArgs>(arguments)..., std::get<indices>(Super::GetPayload())...);
	}

	lib::SharedPtr<TObjectType> m_object;
	TFuncType					m_function;
};


template<typename TObjectType, typename TFuncType, typename... TArgs>
class WeakMemberFunctionBinding {};

template<typename TObjectType, typename TFuncType, typename TReturnType, typename... TArgs, typename... TPayload>
class WeakMemberFunctionBinding<TObjectType, TFuncType, TReturnType(TArgs...), TPayload...> : public DelegateBindingWithPayload<TReturnType(TArgs...), TPayload...>
{
protected:

	using Super = DelegateBindingWithPayload<TReturnType(TArgs...), TPayload...>;

public:

	template<typename... TInputPayload>
	WeakMemberFunctionBinding(const lib::SharedPtr<TObjectType>& object, TFuncType function, TInputPayload&&... payload)
		: Super(std::forward<TInputPayload>(payload)...)
		, m_object(object)
		, m_function(function)
	{ }

	virtual Bool IsValid() const override
	{
		return !m_object.expired() && m_function != nullptr;
	}

	virtual TReturnType Execute(TArgs&&... arguments) override
	{
		return ExecuteImpl(std::forward<TArgs>(arguments)..., std::index_sequence_for<TPayload...>{});
	}

private:

	template<SizeType... indices>
	TReturnType ExecuteImpl(TArgs&&... arguments, std::index_sequence<indices...>)
	{
		return (m_object.lock().get()->*m_function)(std::forward<TArgs>(arguments)..., std::get<indices>(Super::GetPayload())...);
	}

	lib::WeakPtr<TObjectType>	m_object;
	TFuncType					m_function;
};


template<typename TCallable, typename... TArgs>
class LambdaBinding {};

template<typename TCallable, typename TReturnType, typename... TArgs, typename... TPayload>
class LambdaBinding<TCallable, TReturnType(TArgs...), TPayload...> : public DelegateBindingWithPayload<TReturnType(TArgs...), TPayload...>
{
protected:

	using Super = DelegateBindingWithPayload<TReturnType(TArgs...), TPayload...>;

public:

	template<typename... TInputPayload>
	explicit LambdaBinding(TCallable&& callable, TInputPayload&&... payload)
		: Super(std::forward<TInputPayload>(payload)...)
		, m_callable(std::forward<TCallable>(callable))
	{ }

	virtual Bool IsValid() const override
	{
		return true;
	}

	virtual TReturnType Execute(TArgs&&... arguments) override
	{
		return ExecuteImpl(std::forward<TArgs>(arguments)..., std::index_sequence_for<TPayload...>{});
	}

private:

	template<SizeType... indices>
	TReturnType ExecuteImpl(TArgs&&... arguments, std::index_sequence<indices...>)
	{
		return m_callable(std::forward<TArgs>(arguments)..., std::get<indices>(Super::GetPayload())...);
	}

	TCallable m_callable;
};

} // spt::lib