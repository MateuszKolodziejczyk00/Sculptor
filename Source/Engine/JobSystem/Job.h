#pragma once

#include "SculptorCoreTypes.h"
#include "SculptorLib/Utility/Templates/Filter.h"


namespace spt::js
{

namespace impl
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
	using Type = std::tuple<void>;
};


template<typename TPrerequisite>
struct PrerequisitesResults<std::tuple<TPrerequisite>>
{
	using Type = std::tuple<typename TPrerequisite::ResultType>;
};


template<typename TPrerequisite, typename... TPrerequisites>
struct PrerequisitesResults<std::tuple<TPrerequisite, TPrerequisites...>>
{
	using Type = lib::TuplePushFront<typename TPrerequisite::ResultType, typename PrerequisitesResults<std::tuple<TPrerequisites...>>::Type>;
};


template<>
struct PrerequisitesResults<std::tuple<void>>
{
	using Type = std::tuple<void>;
};


template<typename TCallable, typename... TArgs>
struct JobInvokeResult
{
	using Type = std::invoke_result_t<TCallable&, TArgs&...>;
};


template<typename TCallable>
struct JobInvokeResult<TCallable, void>
{
	using Type = std::invoke_result_t<TCallable&>;
};


template<typename TCallable, typename TReturnType>
class JobCaller
{
public:

	template<typename TArgs>
	void Invoke(TCallable& callable, TArgs&& args)
	{
		new (reinterpret_cast<TReturnType*>(m_inlineStorage)) TReturnType(std::apply(callable, args));
	}

	const TReturnType& GetResult() const
	{
		return *reinterpret_cast<const TReturnType*>(m_inlineStorage);
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


template<typename TPrerequisite>
struct GetResult
{
	static_assert(impl::isJobWithResult<TPrerequisite>, "Prerequisite must have result type");
	using Type = TPrerequisite;

	Type& prerequisiteRef;
};


template<typename TPrerequisite>
struct ShouldGetResult
{
	static constexpr Bool value = false;
};


template<typename TPrerequisite>
struct ShouldGetResult<GetResult<TPrerequisite>>
{
	static constexpr Bool value = true;
};


template<typename TPrerequisiteWrapped>
struct UnwrapPrerequisite
{
	using Type = TPrerequisiteWrapped;
};


template<typename TPrerequisiteWrapped>
struct UnwrapPrerequisite<GetResult<TPrerequisiteWrapped>>
{
	using Type = TPrerequisiteWrapped;
};


template<typename TPrerequisitesWrapped>
struct UnwrapPrerequisites
{
	using Type = TPrerequisitesWrapped;
};


template<typename... TPrerequisitesWrapped>
struct UnwrapPrerequisites<std::tuple<TPrerequisitesWrapped...>>
{
	using Type = std::tuple<typename UnwrapPrerequisite<TPrerequisitesWrapped>::Type...>;
};

} // impl

class JobInstanceBase abstract
{
public:

	JobInstanceBase() = default;
	virtual ~JobInstanceBase() = default;

	virtual void Execute() { SPT_CHECK_NO_ENTRY(); }
};


template<typename TCallable, typename... TArguments>
class JobInstance
{ };


template<typename TCallable, typename... TArguments>
class JobInstance<TCallable, std::tuple<TArguments...>> : public JobInstanceBase
{
public:

	using ResultType = typename impl::JobInvokeResult<TCallable, TArguments...>::Type;
	using JobCaller = impl::JobCaller<TCallable, ResultType>;

	explicit JobInstance(TCallable&& callable)
		: m_callable(std::forward<TCallable>(callable))
	{ }

	virtual void Execute() override
	{
		//Invoke();
	}

	template<typename... TArgs>
	void Invoke(const TArgs&... args)
	{
		m_caller.Invoke(m_callable, std::forward_as_tuple(args...));
	}

	const ResultType& GetResult() const
	{
		return m_caller.GetResult();
	}

private:

	JobCaller m_caller;
	TCallable m_callable;
};


class JobBase abstract
{
public:

	void Execute()
	{
		m_instance->Execute();
	}

protected:

	explicit JobBase(lib::SharedRef<JobInstanceBase> inInstance)
		: m_instance(std::move(inInstance))
	{ }

private:

	lib::SharedRef<JobInstanceBase> m_instance;
};


template<typename TJobInstance>
class Job : public JobBase
{
public:

	template<typename... TArgs>  
	explicit Job(TArgs&&... args)
		: JobBase(lib::SharedRef<JobInstanceBase>(new TJobInstance(std::forward<TArgs>(args)...)))
	{ }
};


template<typename TPrerequisite>
impl::GetResult<TPrerequisite> GetResult(TPrerequisite& prerequisite)
{
	return impl::GetResult<TPrerequisite>(prerequisite);
}


class JobBuilder
{
public:

	template<typename TCallable, typename... TStaticPrerequisites>
	static auto BuildJob(TCallable&& callable, TStaticPrerequisites&&... prerequisites)
	{
		using PrerequisitesWithResultWrapped = typename lib::Filter<impl::ShouldGetResult, TStaticPrerequisites...>::Type;
		using PrerequisitesWithResult = typename impl::UnwrapPrerequisites<PrerequisitesWithResultWrapped>::Type;
		using Arguments = typename impl::PrerequisitesResults<PrerequisitesWithResult>::Type;

		using JobInstanceType = JobInstance<TCallable, Arguments>;

		return Job<JobInstanceType>(std::forward<TCallable>(callable));
	}
};

} // spt::js