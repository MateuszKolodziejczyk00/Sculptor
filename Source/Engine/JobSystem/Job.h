#pragma once

#include "SculptorCoreTypes.h"
#include "JobTypes.h"
#include "SculptorLib/Utility/Templates/Filter.h"
#include "Scheduler.h"
#include "Utility/Templates/TypeStorage.h"
#include "MathUtils.h"
#include "ThreadInfo.h"
#include "Event.h"


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
		m_returnTypeStorage.Destroy();
	}

	template<typename... TArgs>
	void ConstructResult(TArgs&&... args)
	{
		m_returnTypeStorage.Construct(std::forward<TArgs>(args)...);
	}

	const TReturnType& GetResult() const
	{
		return m_returnTypeStorage.Get();
	}
	
protected:

	TReturnType& GetResultMutable()
	{
		return m_returnTypeStorage.Get();
	}

private:

	virtual const void* GetResultPtr(SizeType size) const
	{
		SPT_CHECK(size == m_returnTypeStorage.GetSize());
		return m_returnTypeStorage.GetAddress();
	}

	lib::TypeStorage<TReturnType> m_returnTypeStorage;
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

	void Invoke() override
	{
		if constexpr (!std::is_same_v<TReturnType, void>)
		{
			JobCallableWithResult<TReturnType>::ConstructResult(InvokeImpl());
		}
		else
		{
			InvokeImpl();
		}
	}

private:

	decltype(auto) InvokeImpl()
	{
		return std::invoke(m_callable);
	}

	TCallable m_callable;
};


template<typename TType>
struct JobInstanceGetter
{
public:

	static lib::SharedPtr<JobInstance> GetInstance(const TType& job)
	{
		SPT_CHECK_NO_ENTRY();
		return lib::SharedPtr<JobInstance>{};
	}
};


#define  SPT_JS_JOB_ALL_SEQENCIAL_MEMORY_ORDER 0

#if SPT_JS_JOB_ALL_SEQENCIAL_MEMORY_ORDER
constexpr auto MemoryOrderSequencial		= std::memory_order_seq_cst;
constexpr auto MemoryOrderAcquire			= std::memory_order_seq_cst;
constexpr auto MemoryOrderRelease			= std::memory_order_seq_cst;
constexpr auto MemoryOrderAcquireRelease	= std::memory_order_seq_cst;
#else
constexpr auto MemoryOrderSequencial		= std::memory_order_seq_cst;
constexpr auto MemoryOrderAcquire			= std::memory_order_acquire;
constexpr auto MemoryOrderRelease			= std::memory_order_release;
constexpr auto MemoryOrderAcquireRelease	= std::memory_order_acq_rel;
#endif // SPT_JS_SEQENCIAL_MEMORY_ORDER

} // impl

class JobInstance;


template<typename... TJobs>
auto Prerequisites(const TJobs&... jobs)
{
	constexpr SizeType size = lib::ParameterPackSize<TJobs...>::Count;

	using PrerequsitesRangeType = lib::StaticArray<lib::SharedPtr<JobInstance>, size>;

	const auto getJobInstance = []<typename TJobType>(const TJobType& job) -> lib::SharedPtr<JobInstance>
	{
		return impl::JobInstanceGetter<TJobType>::GetInstance(job);
	};

	return PrerequsitesRangeType{ getJobInstance(jobs)... };
}


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

		std::atomic_thread_fence(impl::MemoryOrderRelease);
	}

	template<typename TCallable>
	void SetCallable(TCallable&& callable)
	{
		DestroyCallable();
		
		using ResultType = typename impl::JobInvokeResult<TCallable>::Type;
		using CallableType = impl::JobCallable<std::decay_t<TCallable>, ResultType>;

		constexpr SizeType callableAlignment = alignof(CallableType);

		Bool allocatedInline = false;

		if constexpr (sizeof(CallableType) <= s_inlineStorageSize)
		{
			const SizeType inlineStorageAddress = reinterpret_cast<SizeType>(m_inlineStorage);
			const SizeType inlineAllocationAddress = math::Utils::RoundUp(inlineStorageAddress, callableAlignment);
			if (inlineStorageAddress + s_inlineStorageSize >= inlineAllocationAddress + sizeof(CallableType))
			{
				m_callable = new (reinterpret_cast<void*>(inlineAllocationAddress)) CallableType(std::move(callable));
				allocatedInline = true;
			}
		}

		if (!allocatedInline)
		{
			m_callable = new CallableType(std::move(callable));
		}

		std::atomic_thread_fence(impl::MemoryOrderRelease);
	}

	void Invoke()
	{
		std::atomic_thread_fence(impl::MemoryOrderAcquire);

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
			if (   reinterpret_cast<void*>(m_callable) >= reinterpret_cast<void*>(m_inlineStorage)
				&& reinterpret_cast<void*>(m_callable) < reinterpret_cast<void*>(m_inlineStorage + s_inlineStorageSize))
			{
				m_callable->~JobCallableBase();
			}
			else
			{
				delete m_callable;
			}

			m_callable = nullptr;
		}
	}

	alignas(8) Byte			m_inlineStorage[s_inlineStorageSize];
	impl::JobCallableBase*	m_callable;
};


class JobInstance : public std::enable_shared_from_this<JobInstance>
{
	enum class EJobState : Uint8
	{
		Pending,
		Executing,
		Executed,
		Finished
	};

public:

	explicit JobInstance(const char* name)
		: m_remainingPrerequisitesNum(0)
		, m_jobState(EJobState::Pending)
		, m_priority(EJobPriority::Default)
		, m_flags(EJobFlags::Default)
		, m_name(name)
	{ }

	template<typename TCallable>
	void Init(TCallable&& callable, EJobPriority::Type priority, EJobFlags flags)
	{
		SPT_PROFILER_FUNCTION();

		SetCallable(std::forward<TCallable>(callable));

		m_priority = priority;
		m_flags = flags;

		OnConstructed();
	}

	template<typename TCallable, typename TPrerequisitesRange>
	void Init(TCallable&& callable, TPrerequisitesRange&& prerequisites, EJobPriority::Type priority, EJobFlags flags)
	{
		SPT_PROFILER_FUNCTION();

		SetCallable(std::forward<TCallable>(callable));

		m_priority = priority;
		m_flags = flags;
		
		AddPrerequisites(std::forward<TPrerequisitesRange>(prerequisites));

		OnConstructed();
	}

	Bool TryExecute()
	{
		const Int32 remainingPrerequisites = m_remainingPrerequisitesNum.load(impl::MemoryOrderAcquire);
		if (remainingPrerequisites > 0)
		{
			return false;
		}

		EJobState previous = EJobState::Pending;
		const Bool executeThisThread = m_jobState.compare_exchange_strong(previous, EJobState::Executing, impl::MemoryOrderSequencial);

		Bool finishedThisThread = false;

		if (executeThisThread)
		{
			Execute();

			if (!CanFinishExecution())
			{
				// We can use lockless function because we're on executing thread
				TryExecutePrerequisites_Lockless(m_prerequisites);
			}

			m_jobState.store(EJobState::Executed, impl::MemoryOrderSequencial);

			if (CanFinishExecution())
			{
				finishedThisThread = TryFinish();
				if (!finishedThisThread)
				{
					// This thread couldn't finish this job, but we should return true if job was already finished
					previous = m_jobState.load(impl::MemoryOrderSequencial);
				}
			}
		}

		return (executeThisThread && finishedThisThread) || previous == EJobState::Finished;
	}

	Bool IsFinished() const
	{
		return m_jobState.load(impl::MemoryOrderSequencial) == EJobState::Finished;
	}

	void Wait()
	{
		const EJobState loadedState = m_jobState.load(impl::MemoryOrderSequencial);
		if (loadedState == EJobState::Finished)
		{
			return;
		}

		if (loadedState == EJobState::Pending)
		{
			TryExecutePrerequisites();

			if (m_remainingPrerequisitesNum.load() == 0)
			{
				const Bool executed = TryExecute();
				if (executed)
				{
					return;
				}
			}
		}
		else if (loadedState == EJobState::Executing)
		{
			// Try executing nested jobs
			TryExecutePrerequisites();
		}

		platf::Event finishEvent(true);

		lib::SharedRef<JobInstance> finishEventJob = lib::MakeShared<JobInstance>("WaitEventJob");
		finishEventJob->Init([&finishEvent] { finishEvent.Trigger(); }, Prerequisites(shared_from_this()), GetPriority(), js::EJobFlags::None);
		
		finishEvent.Wait();
	}

	template<typename TResultType>
	const TResultType& GetResultAs() const
	{
		SPT_CHECK(IsFinished());
		return m_callable.GetResult<TResultType>();
	}

	EJobPriority::Type GetPriority() const
	{
		return m_priority;
	}

	EJobFlags GetFlags() const
	{
		return m_flags;
	}

	void AddNested(lib::SharedPtr<JobInstance> job)
	{
		AddPrerequisite(std::move(job));
	}

private:

	template<typename TCallable>
	void SetCallable(TCallable&& callable)
	{
		m_callable.SetCallable(callable);
	}

	template<typename TPrerequisitesRange>
	void AddPrerequisites(TPrerequisitesRange&& prerequisites)
	{
		SPT_PROFILER_FUNCTION();

		// we don't require any synchronization here - it's called only locally during job initialization
		m_remainingPrerequisitesNum.fetch_add(static_cast<Int32>(prerequisites.size()));

		m_prerequisites.reserve(prerequisites.size());

		for (auto& prerequisite : prerequisites)
		{
			m_prerequisites.emplace_back(prerequisite);
			prerequisite->AddConsequent(shared_from_this());
		}
	}

	void AddPrerequisite(const lib::SharedPtr<JobInstance>& job)
	{
		const Bool useLock = m_jobState.load(impl::MemoryOrderSequencial) != EJobState::Pending;
		if (useLock)
		{
			m_prerequisitesLock.lock();
		}

		m_remainingPrerequisitesNum.fetch_add(1, impl::MemoryOrderAcquireRelease);
		m_prerequisites.emplace_back(job);
		job->AddConsequent(shared_from_this());

		if (useLock)
		{
			m_prerequisitesLock.unlock();
		}
	}

	void AddConsequent(lib::SharedPtr<JobInstance> next)
	{
		SPT_CHECK(!!next);

		const lib::LockGuard lockGuard(m_consequentsLock);

		if(m_jobState.load(impl::MemoryOrderSequencial) != EJobState::Finished)
		{
			m_consequents.emplace_back(std::move(next));
		}
		else
		{
			next->PostPrerequisiteExecuted();
		}
	}

	void OnConstructed()
	{
		if (lib::HasAnyFlag(GetFlags(), EJobFlags::Inline))
		{
			Wait();
		}
		else
		{
			if (m_remainingPrerequisitesNum.load() == 0)
			{
				Schedule();
			}
		}
	}

	void TryExecutePrerequisites() const
	{
		if (!CanFinishExecution())
		{
			const Bool canUseLockless = m_jobState.load(impl::MemoryOrderSequencial) != EJobState::Executing;
			if (canUseLockless)
			{
				TryExecutePrerequisites_Lockless(m_prerequisites);
			}
			else
			{
				TryExecutePrerequisites_Locked();
			}
		}
	}

	void TryExecutePrerequisites_Lockless(const lib::DynamicArray<lib::SharedPtr<JobInstance>>& prerequisitesToExecute) const
	{
		for (const lib::SharedPtr<JobInstance>& prerequisite : m_prerequisites)
		{
			if (CanFinishExecution())
			{
				break;
			}

			prerequisite->TryExecute();
		}
	}

	void TryExecutePrerequisites_Locked() const
	{
		lib::DynamicArray<lib::SharedPtr<JobInstance>> prerequisitesCopy;
		{
			const lib::LockGuard lockGuard(m_prerequisitesLock);
			prerequisitesCopy = m_prerequisites;
		}

		TryExecutePrerequisites_Lockless(prerequisitesCopy);
	}

	void Execute()
	{
		const JobExecutionScope executionScope(*this);

		m_prerequisites.clear();

		m_remainingPrerequisitesNum.fetch_add(1, impl::MemoryOrderRelease);
		DoExecute();
		m_remainingPrerequisitesNum.fetch_add(-1, impl::MemoryOrderRelease);
	}

	void DoExecute()
	{
		m_callable.Invoke();
	}

	inline Bool CanFinishExecution() const
	{
		return m_remainingPrerequisitesNum.load(impl::MemoryOrderAcquire) == 0;
	}

	Bool TryFinish()
	{
		EJobState expected = EJobState::Executed;
		const Bool finishThisThread = m_jobState.compare_exchange_strong(expected, EJobState::Finished, impl::MemoryOrderSequencial);

		SPT_CHECK(expected != EJobState::Pending);

		if (finishThisThread)
		{
			m_jobState.store(EJobState::Finished, impl::MemoryOrderSequencial);

			m_prerequisites.clear();

			lib::DynamicArray<lib::SharedPtr<JobInstance>> consequentsCopy;
			{
				const lib::LockGuard lockGuard(m_consequentsLock);

				// During next for loop we this job may be destroyed during call to PostPrerequisiteExecuted if consequent had only reference to this job
				// This may happen if this job is nested
				// Because of this we don't want to use any members from now on, so we need to move consequents to local array
				consequentsCopy = std::move(m_consequents);
			}

			for (const lib::SharedPtr<JobInstance>& consequent : consequentsCopy)
			{
				consequent->PostPrerequisiteExecuted();
			}
		}

		return finishThisThread || expected == EJobState::Finished;
	}

	void PostPrerequisiteExecuted()
	{
		const Int32 remaining = m_remainingPrerequisitesNum.fetch_add(-1, impl::MemoryOrderAcquireRelease) - 1;
		SPT_CHECK(remaining >= 0);

		if (remaining == 0)
		{
			const EJobState currentState = m_jobState.load(impl::MemoryOrderSequencial);
			if (currentState == EJobState::Pending)
			{
				Schedule();
			}
			else if(currentState == EJobState::Executed)
			{
				// Finished nested jobs
				TryFinish();
			}
		}
	}

	void Schedule()
	{
		Scheduler::ScheduleJob(shared_from_this());
	}

	lib::DynamicArray<lib::SharedPtr<JobInstance>>  m_prerequisites;
	std::atomic<Int32>								m_remainingPrerequisitesNum;
	mutable lib::Lock								m_prerequisitesLock;

	std::atomic<EJobState> m_jobState;

	lib::DynamicArray<lib::SharedPtr<JobInstance>>	m_consequents;
	lib::Lock										m_consequentsLock;

	JobCallableWrapper	m_callable;

	EJobPriority::Type	m_priority;
	EJobFlags			m_flags;

	const char* m_name;
};


class JobInstanceBuilder
{
public:

	template<typename TCallable, typename TPrerequisitesRange>
	static auto Build(const char* name, TCallable&& callable, TPrerequisitesRange&& prerequisites, EJobPriority::Type priority, EJobFlags flags)
	{
		SPT_PROFILER_FUNCTION();

		lib::SharedPtr<JobInstance> job;
		{
			SPT_PROFILER_SCOPE("Allocate Job");
			job = lib::MakeShared<JobInstance>(name);
		}
		job->Init(std::forward<TCallable>(callable), std::forward<TPrerequisitesRange>(prerequisites), priority, flags);
		return lib::Ref(job);
	}

	template<typename TCallable>
	static auto Build(const char* name, TCallable&& callable, EJobPriority::Type priority, EJobFlags flags)
	{
		SPT_PROFILER_FUNCTION();

		lib::SharedPtr<JobInstance> job;
		{
			SPT_PROFILER_SCOPE("Allocate Job");
			job = lib::MakeShared<JobInstance>(name);
		}
		job->Init(std::forward<TCallable>(callable), priority, flags);
		return lib::Ref(job);
	}
};


class Job
{
public:

	explicit Job(lib::SharedRef<JobInstance> inInstance)
		: m_instance(std::move(inInstance))
	{ }

	void Wait() const
	{
		m_instance->Wait();
	}

	const lib::SharedRef<JobInstance>& GetJobInstance() const
	{
		return m_instance;
	}

	template<typename TCallable>
	Job Then(const char* name, TCallable&& callable)
	{
		lib::SharedRef<JobInstance> instance =  JobInstanceBuilder::Build(name, callable, Prerequisites(*this), m_instance->GetPriority(), m_instance->GetFlags());
		return Job(std::move(instance));
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

	const TResultType& Await() const
	{
		Wait();
		return GetJobInstance()->GetResultAs<TResultType>();
	}
};


template<typename TResultType>
JobWithResult<TResultType> AsJobWithResult(const Job& job)
{
	return JobWithResult<TResultType>(job.GetJobInstance());
}


namespace impl
{

template<>
struct JobInstanceGetter<Job>
{
public:

	static lib::SharedPtr<JobInstance> GetInstance(const Job& job)
	{
		return job.GetJobInstance();
	}
};

template<typename TResultType>
struct JobInstanceGetter<JobWithResult<TResultType>> : public JobInstanceGetter<Job> {};

template<>
struct JobInstanceGetter<lib::SharedPtr<JobInstance>>
{
public:

	static lib::SharedPtr<JobInstance> GetInstance(const lib::SharedPtr<JobInstance>& job)
	{
		return job;
	}
};

} // impl


class JobBuilder
{
public:

	template<typename TCallable>
	static auto Build(const lib::SharedRef<JobInstance>& job)
	{
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