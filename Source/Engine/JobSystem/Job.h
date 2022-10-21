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


template<typename TCallable, typename TReturnType>
class JobCaller
{
public:

	void Invoke(TCallable& callable)
	{
		new (reinterpret_cast<TReturnType*>(m_inlineStorage)) TReturnType(std::invoke(callable));
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

	void Invoke(TCallable& callable)
	{
		callable();
	}

	void GetResult() const
	{ }
};

} // impl

class JobInstanceBase;


template<typename T>
lib::SharedPtr<JobInstanceBase> GetJobSharedPtr(const T& val)
{
	SPT_CHECK_NO_ENTRY();

	return lib::SharedPtr<JobInstanceBase>{};
}


class JobInstanceBase abstract
{
	enum class EJobState : Uint8
	{
		Pending,
		Executing,
		Finished
	};

public:

	JobInstanceBase()
		: m_remainingPrerequisitesNum(0)
		, m_jobState(EJobState::Pending)
	{ }

	virtual ~JobInstanceBase() = default;

	template<typename TPrerequisitesRange>
	void AddPrerequisites(TPrerequisitesRange&& prerequisites)
	{
		// we don't require any synchronization here - it's called only locally during job initialization
		m_remainingPrerequisitesNum.store(static_cast<Int32>(prerequisites.size()));

		m_prerequisites.reserve(prerequisites.size());

		for (auto& prerequisite : prerequisites)
		{
			lib::SharedPtr<JobInstanceBase> prerequisitePtr = GetJobSharedPtr(prerequisite);

			m_prerequisites.emplace_back(prerequisite);

			prerequisitePtr->AddConsequent(this);
		}
	}

	void OnConstructed()
	{
		if (m_remainingPrerequisitesNum.load() == 0)
		{
			Schedule();
		}
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

	void AddConsequent(JobInstanceBase* next)
	{
		SPT_CHECK(!!next);

		const lib::LockGuard lockGuard(m_consequentsLock);

		if(m_jobState.load() != EJobState::Finished)
		{
			m_consequents.emplace_back(next);
		}
	}

	inline void AddNested()
	{
		m_remainingPrerequisitesNum.fetch_add(1);
	}

	inline void PostPrerequisiteExecuted()
	{
		const Int32 remaining = m_remainingPrerequisitesNum.fetch_add(-1);

		if (remaining == 0)
		{
			Schedule();
		}
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
			for (const lib::SharedPtr<JobInstanceBase>& prerequisite : m_prerequisites)
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

		// add consequent;
		SPT_CHECK_NO_ENTRY();
		
		lib::UnlockableLockGuard lockGuard(lock);
		cv.wait(lockGuard, [this] { return m_jobState.load() == EJobState::Finished; });
	}

protected:

	virtual void DoExecute()
	{
		SPT_CHECK_NO_ENTRY(); // Implement in child classes
	}

	inline Bool CanFinishExecution()
	{
		return m_remainingPrerequisitesNum.load() == 0;
	}

	void Finish()
	{
		const lib::LockGuard lockGuard(m_consequentsLock);

		for (JobInstanceBase* consequent : m_consequents)
		{
			consequent->PostPrerequisiteExecuted();
		}

		m_jobState.store(EJobState::Finished);
	}

	void Schedule()
	{

	}

private:

	lib::DynamicArray<lib::SharedPtr<JobInstanceBase>>  m_prerequisites;
	std::atomic<Int32>									m_remainingPrerequisitesNum;

	std::atomic<EJobState> m_jobState;

	lib::DynamicArray<JobInstanceBase*>	m_consequents;
	lib::Lock							m_consequentsLock;
};


template<typename TCallable>
class JobInstance : public JobInstanceBase
{
public:

	using ResultType = typename impl::JobInvokeResult<TCallable>::Type;
	using JobCaller = impl::JobCaller<TCallable, ResultType>;

	explicit JobInstance(TCallable&& callable)
		: m_callable(std::forward<TCallable>(callable))
	{ }

	const ResultType& GetResult() const
	{
		return m_caller.GetResult();
	}

private:

	virtual void DoExecute() override
	{
		Invoke();
	}

	void Invoke()
	{
		m_caller.Invoke(m_callable);
	}

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

	template<typename TPrerequisitesRange>
	void AddPrerequisites(const TPrerequisitesRange& range)
	{
		m_instance->AddPrerequisites(range);
	}

	void OnConstructed()
	{
		m_instance->OnConstructed();
	}

	const lib::SharedRef<JobInstanceBase>& GetJobInstance() const
	{
		return m_instance;
	}

protected:

	explicit JobBase(lib::SharedRef<JobInstanceBase> inInstance)
		: m_instance(std::move(inInstance))
	{ }
	
	template<typename TJobType>
	const TJobType& GetJob() const
	{
		static_assert(std::is_base_of_v<JobInstanceBase, TJobType>);
		return static_cast<const TJobType&>(*m_instance);
	}

private:

	lib::SharedRef<JobInstanceBase> m_instance;
};


template<typename TJobInstance>
class Job : public JobBase
{
public:

	using JobInstanceType = TJobInstance;
	using ResultType = typename TJobInstance::ResultType;

	template<typename... TArgs>  
	explicit Job(TArgs&&... args)
		: JobBase(lib::SharedRef<JobInstanceBase>(new TJobInstance(std::forward<TArgs>(args)...)))
	{ }

	decltype(auto) GetResult() const
	{
		return GetJob<JobInstanceType>().GetResult();
	}
};

template<typename T>
lib::SharedPtr<Job<T>> GetJobSharedPtr(const Job<T>& val)
{
	return val.GetJobInstance();
}


class JobBuilder
{
public:

	template<typename TCallable, typename TPrerequisitesRange>
	static auto BuildJob(TCallable&& callable, const TPrerequisitesRange& prerequisites)
	{
		using JobInstanceType = JobInstance<TCallable>;

		Job<JobInstanceType> job(std::forward<TCallable>(callable));
		job.AddPrerequisites(prerequisites);
		job.OnConstructed();

		return job;
	}

	template<typename TCallable>
	static auto BuildJob(TCallable&& callable)
	{
		using JobInstanceType = JobInstance<TCallable>;

		Job<JobInstanceType> job(std::forward<TCallable>(callable));
		job.OnConstructed();

		return job;
	}
};

} // spt::js