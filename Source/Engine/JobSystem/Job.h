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


template<typename TCallable>
struct JobInvokeResult
{
	using Type = std::invoke_result_t<TCallable&>;
};


class JobCallableBase
{
public:

	JobCallableBase() = default;
	virtual ~JobCallableBase() = default;

	virtual void Invoke() = 0;

	template<typename TType>
	const TType& GetResultAs() const
	{
		const TType* result = static_cast<const TType*>(GetResultPtr(sizeof(TType)));
		SPT_CHECK(!!result);
		return *result;
	}

protected:

	virtual const void* GetResultPtr(SizeType size) const
	{
		return nullptr;
	}
};


template<typename TReturnType>
class JobCallableWithResult : public JobCallableBase
{
public:

	~JobCallableWithResult()
	{
		GetResultMutable().~TReturnType();
	}

	const TReturnType& GetResult() const
	{
		return *reinterpret_cast<const TReturnType*>(m_inlineStorage);
	}
	
protected:

	TReturnType& GetResultMutable()
	{
		return *reinterpret_cast<TReturnType*>(m_inlineStorage);
	}

private:

	virtual const void* GetResultPtr(SizeType size) const
	{
		SPT_CHECK(size == sizeof(TReturnType));
		return m_inlineStorage;
	}

	alignas(TReturnType) Byte m_inlineStorage[sizeof(TReturnType)];
};


template<>
class JobCallableWithResult<void> : public JobCallableBase
{
public:

};


template<typename TCallable, typename TReturnType>
class JobCallable : public JobCallableWithResult<TReturnType>
{
public:

	explicit JobCallable(TCallable&& callable)
		: m_callable(std::move(callable))
	{ }

	virtual void Invoke() override
	{
		if constexpr (!std::is_same_v<TReturnType, void>)
		{
			JobCallableWithResult<TReturnType>::GetResultMutable() = InvokeImpl();
		}
		else
		{
			InvokeImpl();
		}
	}

private:

	decltype(auto) InvokeImpl() const
	{
		return std::invoke(m_callable);
	}

	TCallable m_callable;
};

} // impl

class JobInstance;


class JobCallableWrapper
{
	static constexpr SizeType s_inlineStorageSize = 32;

public:

	JobCallableWrapper()
		: m_callable(nullptr)
	{ }

	~JobCallableWrapper()
	{
		DestroyCallable();
	}

	template<typename TCallable>
	void SetCallable(TCallable&& callable)
	{
		DestroyCallable();
		
		using ResultType = typename impl::JobInvokeResult<TCallable>::Type;
		using CallableType = impl::JobCallable<std::decay_t<TCallable>, ResultType>;

		if constexpr (sizeof(CallableType) <= s_inlineStorageSize)
		{
			m_callable = new (m_inlineStorage) CallableType(std::move(callable));
		}
		else
		{
			m_callable = new CallableType(std::move(callable));
		}
	}

	void Invoke()
	{
		SPT_CHECK(!!m_callable);

		m_callable->Invoke();
	}

	template<typename TType>
	const TType& GetResult() const
	{
		SPT_CHECK(!!m_callable);
		return m_callable->GetResultAs<TType>();
	}

private:

	void DestroyCallable()
	{
		if (m_callable)
		{
			if (reinterpret_cast<void*>(m_callable) == reinterpret_cast<void*>(m_inlineStorage))
			{
				m_callable->~JobCallableBase();
			}
			else
			{
				delete m_callable;
			}
		}
	}

	alignas(4) Byte			m_inlineStorage[s_inlineStorageSize];
	impl::JobCallableBase*	m_callable;
};


class JobInstance : public std::enable_shared_from_this<JobInstance>
{
	enum class EJobState : Uint8
	{
		Pending,
		Executing,
		Finished
	};

public:

	template<typename TCallable>
	explicit JobInstance(TCallable&& callable)
		: m_remainingPrerequisitesNum(0)
		, m_jobState(EJobState::Pending)
	{
		SetCallable(std::forward<TCallable>(callable));
		OnConstructed();
	}

	template<typename TCallable, typename TPrerequisitesRange>
	JobInstance(TCallable&& callable, TPrerequisitesRange&& prerequisites)
		: m_remainingPrerequisitesNum(0)
		, m_jobState(EJobState::Pending)
	{
		SetCallable(std::forward<TCallable>(callable));
		AddPrerequisites(std::forward<TPrerequisitesRange>(prerequisites));
		OnConstructed();
	}

	template<typename TCallable>
	JobInstance(TCallable&& callable, lib::SharedPtr<JobInstance> prerequisite)
		: m_remainingPrerequisitesNum(0)
		, m_jobState(EJobState::Pending)
	{
		SetCallable(std::forward<TCallable>(callable));
		AddPrerequisite(std::forward<lib::SharedPtr<JobInstance>>(prerequisite));
		OnConstructed();
	}

	virtual ~JobInstance() = default;

	template<typename TCallable>
	void SetCallable(TCallable&& callable)
	{
		m_callable.SetCallable(callable);
	}

	template<typename TPrerequisitesRange>
	void AddPrerequisites(TPrerequisitesRange&& prerequisites)
	{
		// we don't require any synchronization here - it's called only locally during job initialization
		m_remainingPrerequisitesNum.fetch_add(static_cast<Int32>(prerequisites.size()));

		m_prerequisites.reserve(prerequisites.size());

		for (auto& prerequisite : prerequisites)
		{
			m_prerequisites.emplace_back(prerequisite);
			prerequisite->AddConsequent(this);
		}
	}

	void AddPrerequisite(lib::SharedPtr<JobInstance> job)
	{
		m_remainingPrerequisitesNum.fetch_add(1);
		m_prerequisites.emplace_back(std::forward<lib::SharedPtr<JobInstance>>(job));
	}

	void AddConsequent(JobInstance* next)
	{
		SPT_CHECK(!!next);

		const lib::LockGuard lockGuard(m_consequentsLock);

		if(m_jobState.load() != EJobState::Finished)
		{
			m_consequents.emplace_back(next);
		}
	}

	inline void AddNested(lib::SharedPtr<JobInstance> job)
	{
		m_remainingPrerequisitesNum.fetch_add(1);

		// added only on thread that creates job, on or thread executing job, so we don't need lock
		m_prerequisites.emplace_back(std::forward<lib::SharedPtr<JobInstance>>(job));
	}

	Bool Execute()
	{
		EJobState expected = EJobState::Pending;
		const Bool executeThisThread = m_jobState.compare_exchange_strong(expected, EJobState::Executing);

		if (executeThisThread)
		{
			SPT_CHECK(m_remainingPrerequisitesNum.load() == 0);
			DoExecute();

			if (CanFinishExecution())
			{
				Finish();
			}
		}

		return executeThisThread || expected == EJobState::Finished;
	}

	void Wait()
	{
		const EJobState loadedState = m_jobState.load();
		if (loadedState == EJobState::Finished)
		{
			return;
		}

		if (loadedState == EJobState::Pending)
		{
			// We probably don't need lock here for now
			for (const lib::SharedPtr<JobInstance>& prerequisite : m_prerequisites)
			{
				prerequisite->Execute();
			}

			if (m_remainingPrerequisitesNum.load() == 0)
			{
				const Bool executed = Execute();
				if (executed)
				{
					return;
				}
			}
		}

		std::condition_variable cv;
		lib::Lock lock;

		const lib::SharedRef<JobInstance> finishEvent = lib::MakeShared<JobInstance>([&cv] { cv.notify_one(); }, shared_from_this());
		
		lib::UnlockableLockGuard lockGuard(lock);
		cv.wait(lockGuard, [this] { return m_jobState.load() == EJobState::Finished; });
	}

	template<typename TResultType>
	const TResultType& GetResultAs() const
	{
		return m_callable.GetResult<TResultType>();
	}

protected:

	void OnConstructed()
	{
		if (m_remainingPrerequisitesNum.load() == 0)
		{
			Schedule();
		}
	}

	virtual void DoExecute()
	{
		m_callable.Invoke();
	}

	inline Bool CanFinishExecution()
	{
		return m_remainingPrerequisitesNum.load() == 0;
	}

	void Finish()
	{
		const lib::LockGuard lockGuard(m_consequentsLock);

		for (JobInstance* consequent : m_consequents)
		{
			consequent->PostPrerequisiteExecuted();
		}

		m_jobState.store(EJobState::Finished);
	}

	inline void PostPrerequisiteExecuted()
	{
		const Int32 remaining = m_remainingPrerequisitesNum.fetch_add(-1);

		if (remaining == 0)
		{
			Schedule();
		}
	}

	void Schedule()
	{

	}

private:

	lib::DynamicArray<lib::SharedPtr<JobInstance>>  m_prerequisites;
	std::atomic<Int32>								m_remainingPrerequisitesNum;

	std::atomic<EJobState> m_jobState;

	lib::DynamicArray<JobInstance*>	m_consequents;
	lib::Lock							m_consequentsLock;

	JobCallableWrapper	m_callable;
};


class Job
{
public:

	explicit Job(lib::SharedRef<JobInstance> inInstance)
		: m_instance(std::move(inInstance))
	{ }

	void Wait()
	{
		m_instance->Wait();
	}

	const lib::SharedRef<JobInstance>& GetJobInstance() const
	{
		return m_instance;
	}

private:

	lib::SharedRef<JobInstance> m_instance;
};


template<typename TResultType>
class JobWithResult : public Job
{
public:

	explicit JobWithResult(lib::SharedRef<JobInstance> inInstance)
		: Job(std::move(inInstance))
	{ }

	const TResultType& GetResult() const
	{
		return GetJobInstance()->GetResultAs<TResultType>();
	}
};


template<typename... TJobs>
auto Prerequisites(const TJobs&... jobs)
{
	constexpr SizeType size = lib::ParameterPackSize<TJobs...>::Count;

	using PrerequsitesRangeType = lib::StaticArray<lib::SharedPtr<JobInstance>, size>;

	const auto getJobInstance = []<typename T>(const T& job)
	{
		static_assert(std::is_base_of_v<Job, T>);
		return job.GetJobInstance();
	};

	return PrerequsitesRangeType{ getJobInstance(jobs)... };
}


class JobBuilder
{
public:

	template<typename TCallable, typename TPrerequisitesRange>
	static auto BuildJob(TCallable&& callable, TPrerequisitesRange&& prerequisites)
	{
		SPT_PROFILER_FUNCTION();

		lib::SharedRef<JobInstance> job = lib::MakeShared<JobInstance>(std::forward<TCallable>(callable), std::forward<TPrerequisitesRange>(prerequisites));
		return CreateJobWrapper<TCallable>(job);
	}

	template<typename TCallable>
	static auto BuildJob(TCallable&& callable)
	{
		SPT_PROFILER_FUNCTION();

		lib::SharedRef<JobInstance> job = lib::MakeShared<JobInstance>(std::forward<TCallable>(callable));
		return CreateJobWrapper<TCallable>(job);
	}

private:

	template<typename TCallable>
	static auto CreateJobWrapper(const lib::SharedRef<JobInstance>& jobInstance)
	{
		using CallableResult = impl::JobInvokeResult<TCallable>::Type;
		using JobType = std::conditional_t<std::is_same_v<CallableResult, void>, Job, JobWithResult<CallableResult>>;

		return JobType(jobInstance);
	}
};

} // spt::js