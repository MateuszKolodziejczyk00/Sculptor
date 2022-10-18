#pragma once

#include "SculptorCoreTypes.h"
#include "SculptorLib/Utility/Templates/Filter.h"


namespace spt::js
{

namespace spt::impl
{

template<typename TType>
struct IsJobWithResult
{
	static constexpr Bool value = !std::is_same_v<typename TType::ResultType, void>;
};

template<typename TType>
static constexpr Bool isJobWithResult = IsJobWithResult<TType>::value;


template<typename... TPrerequisites>
struct PrerequisitesResults
{
	using Type = lib::TuplePushFront<
};


template<typename TPrerequisite, typename... TPrerequisites>
struct PrerequisitesResults<std::tuple<TPrerequisite, TPrerequisites...>>
{
	using Type = lib::TuplePushFront<typename TPrerequisite::ResultType, PrerequisitesResults<TPrerequisites...>::Type>;
};


template<>
struct PrerequisitesResults<std::tuple<void>>
{
	using Type = std::tuple<void>;
};


template<typename TCallable, typename TArgs...>
struct JobInvokeResult
{ };


template<typename TCallable, typename TArgs...>
struct JobInvokeResult<TCallable, std::tuple<TArgs...>>
{
	using Type = std::invoke_result_t<TCallable&, TArgs...>;
};


template<typename TCallable, typename TReturnType>
class JobCaller
{
public:

	template<typename TArgs>
	void Inoke(TCallable& callable, TArgs&& args)
	{
		new (m_inlineStorage) TReturnType(std::apply(callable, args));
	}

	TReturnType& GetResult() const
	{
		return *reinterpret_cast<TReturnType*>(m_inlineStorage);
	}

private:

	alignas(TReturnType) Byte m_inlineStorage[sizeof(TReturnType)];
};


template<typename TCallable>
class JobCaller<TCallable, void>
{
public:

	template<typename TArgs>
	void Invoke(TCallable& callable, TArgs&& args)
	{
		callable();
	}

	void GetResult() const
	{ }
};

} // spt::impl

template<typename TCallable, typename... TPrerequisites>
class JobInstance
{
public:

	using PrerequisitesWithResult = lib::Filter<impl::isJobWithResult, TPrerequisites...>;
	using Arguments = impl::PrerequisitesResults<PrerequisitesWithResult>::Type;
	using ResultType = impl::JobInvokeResult<TCallable, Arguments>;
	using JobCaller = impl::JobCaller<TCallable, ResultType>;

	JobInstance(TCallable&& callable)
		: m_callable(std::forward<TCallable>(callable))
	{ }

	void Invoke(const Arguments& args)
	{
		m_caller.Invoke(m_callable, args)
	}

	ResultType& GetResult() const
	{
		return m_caller.GetResult();
	}

private:

	JobCaller m_caller;
	TCallable m_callable;
};

} // spt::js