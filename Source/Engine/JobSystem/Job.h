#pragma once

#include "SculptorCoreTypes.h"
#include "JobTypes.h"
#include "SculptorLib/Utility/Templates/Filter.h"
#include "Scheduler.h"
#include "Utility/Templates/TypeStorage.h"
#include "MathUtils.h"
#include "ThreadInfo.h"
#include "Event.h"
#include "Worker.h"


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

	static lib::MTHandle<JobInstance> GetInstance(const TType& job)
	{
		SPT_CHECK_NO_ENTRY();
		return lib::MTHandle<JobInstance>{};
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

	using PrerequsitesRangeType = lib::StaticArray<lib::MTHandle<JobInstance>, size>;

	const auto getJobInstance = []<typename TJobType>(const TJobType& job) -> lib::MTHandle<JobInstance>
	{
		return impl::JobInstanceGetter<TJobType>::GetInstance(job);
	};

	return PrerequsitesRangeType{ getJobInstance(jobs)... };
}


class JobCallableWrapper
{
	static constexpr SizeType s_inlineStorageSize = 128u;

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


class JobInstance : public lib::MTRefCounted
{
	enum class EJobState : Uint8
	{
		// Job is created but it's waiting for external signal to be ready to be scheduled
		Inactive,
		// Job is created but not scheduled yet
		// That means that we either wait for prerequisites to be finished or job is not initialized yet
		Pending,
		// Job is currently scheduled in one of the queues
		Scheduled,
		// Job is currently executing it's callable
		Executing,
		// Job was executed and it's ready to be finished after all of it's nested jobs will be finished
		Executed,
		// Job is currently finishing (doing cleanup)
		Finishing,
		// Job is finished and ready to be destroyed
		Finished
	};

public:

	explicit JobInstance(const char* name)
		: m_remainingPrerequisitesNum(0)
		, m_jobState(EJobState::Inactive)
		, m_priority(EJobPriority::Default)
		, m_flags(EJobFlags::Default)
		, m_name(name)
	{ }

	~JobInstance()
	{
		SPT_CHECK(m_jobState == EJobState::Finished);
	}

	template<typename TCallable>
	void Init(TCallable&& callable, const JobDefinitionInternal& def)
	{
		SetCallable(std::forward<TCallable>(callable));

		InitInternal(def);

		OnConstructed();
	}

	template<typename TCallable, typename TPrerequisitesRange>
	void Init(TCallable&& callable, TPrerequisitesRange&& prerequisites, const JobDefinitionInternal& def)
	{
		SetCallable(std::forward<TCallable>(callable));

		InitInternal(def);

		AddPrerequisites(std::forward<TPrerequisitesRange>(prerequisites));

		OnConstructed();
	}

	Bool IsSignaled() const
	{
		return m_jobState.load(impl::MemoryOrderSequencial) != EJobState::Inactive;
	}

	void Signal()
	{
		SPT_CHECK(IsEventJob());
		SPT_CHECK(m_jobState.load(impl::MemoryOrderSequencial) == EJobState::Inactive);
		
		Activate();
	}

	Bool TryExecute()
	{
		const Int32 remainingPrerequisites = m_remainingPrerequisitesNum.load(impl::MemoryOrderAcquire);
		if (remainingPrerequisites > 0)
		{
			return false;
		}

		EJobState previous = EJobState::Scheduled;
		Bool executeThisThread = false;

		while (!executeThisThread && (previous == EJobState::Pending || previous == EJobState::Scheduled))
		{
			executeThisThread = m_jobState.compare_exchange_strong(previous, EJobState::Executing, impl::MemoryOrderSequencial);
		}

		Bool finishedThisThread = false;

		if (executeThisThread)
		{
			Execute();

			finishedThisThread = TryFinish();
			if (!finishedThisThread)
			{
				// This thread couldn't finish this job, but we should return true if job was already finished
				previous = m_jobState.load(impl::MemoryOrderSequencial);
			}
		}

		return (executeThisThread && finishedThisThread) || previous == EJobState::Finished;
	}

	Bool IsResultReady() const
	{
		const EJobState state = m_jobState.load(impl::MemoryOrderSequencial);
		return state == EJobState::Finishing || state == EJobState::Finished;
	}

	Bool IsFinished() const
	{
		return m_jobState.load(impl::MemoryOrderSequencial) == EJobState::Finished;
	}

	void Wait(bool activeWait = false)
	{
		SPT_PROFILER_FUNCTION();

		const EJobState loadedState = m_jobState.load(impl::MemoryOrderSequencial);
		if (loadedState == EJobState::Finished)
		{
			return;
		}

		if (loadedState == EJobState::Pending || loadedState == EJobState::Inactive)
		{
			TryExecutePrerequisites_Locked();

			if (m_remainingPrerequisitesNum.load() == 0)
			{
				const Bool executed = TryExecute();
				if (executed)
				{
					return;
				}
			}
		}
		else if(loadedState == EJobState::Scheduled)
		{
			// We cannot execute local jobs here, if they are already scheduled
			// That's because scheduler holds reference to this job, and we have to be sure that all refs are released before task is finished
			// because of this, we have to wait for worker thread to finish this job
			const Bool canExecuteLocally = !IsLocal();
			if (canExecuteLocally)
			{
				if (TryExecute())
				{
					return;
				}
			}
			else
			{
				// At this point, only thing we can do is to try execute one of the jobs during waiting
				// If we know that this job is in global queue, we don't want to execute local jobs to increase chance of executing this job
				const Bool wantsExecuteLocalQueueJobs = !IsForcedToGlobalQueue();

				EJobState currentState = loadedState;
				while (currentState != EJobState::Finishing || currentState != EJobState::Finished)
				{
					Scheduler::TryExecuteScheduledJob(wantsExecuteLocalQueueJobs);
					currentState = m_jobState.load(impl::MemoryOrderSequencial);
				}
			}
		}
		else if (loadedState == EJobState::Executing || loadedState == EJobState::Executed)
		{
			// Try executing nested jobs
			TryExecutePrerequisites_Locked();
		}
		else if (loadedState == EJobState::Finishing)
		{
			while(m_jobState.load(impl::MemoryOrderSequencial) != EJobState::Finished)
			{
				platf::Platform::SwitchToThread();
			}
			return;
		}

		if (activeWait)
		{
			while (m_jobState.load(impl::MemoryOrderSequencial) != EJobState::Finished);
			{
				Worker::TryActiveWait();
			}
		}
		else
		{
			platf::Event finishEvent(true);

			JobInstance finishEventJob("WaitEventJob");
			finishEventJob.AddRef();

			JobDefinitionInternal waitJobDef;
			waitJobDef.priority = GetPriority();
			waitJobDef.flags    = lib::Flags(EJobFlags::Local, EJobFlags::Inline);
			finishEventJob.Init([&finishEvent] { finishEvent.Trigger(); }, Prerequisites(this), waitJobDef);

			finishEvent.Wait();

			// We have to wait for the job to reach "Finished" state before we destroy it
			// We do this without any events because it should be very fast after signaling the event
			// Other option would be to not use local job here, but this would require memory allocation
			while (finishEventJob.m_jobState.load(impl::MemoryOrderSequencial) != EJobState::Finished)
			{
				platf::Platform::SwitchToThread();
			}

			const Bool canDestroy = finishEventJob.Release();
			SPT_CHECK(canDestroy);
		}
	}

	template<typename TResultType>
	const TResultType& GetResultAs() const
	{
		SPT_CHECK(IsResultReady());
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

	const char* GetName() const
	{
		return m_name;
	}

	Bool IsLocal() const
	{
		return lib::HasAnyFlag(GetFlags(), EJobFlags::Local);
	}

	Bool IsInline() const
	{
		return lib::HasAnyFlag(GetFlags(), EJobFlags::Inline);
	}

	Bool IsForcedToGlobalQueue() const
	{
		return lib::HasAnyFlag(GetFlags(), EJobFlags::ForceGlobalQueue);
	}

	Bool IsEventJob() const
	{
		return lib::HasAnyFlag(GetFlags(), EJobFlags::EventJob);
	}

	void AddNested(lib::MTHandle<JobInstance> job)
	{
		AddPrerequisite(std::move(job));
	}

private:

	template<typename TCallable>
	void SetCallable(TCallable&& callable)
	{
		m_callable.SetCallable(std::forward<TCallable>(callable));
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

	void AddPrerequisite(lib::MTHandle<JobInstance> job)
	{
		const EJobState currentState = m_jobState.load(impl::MemoryOrderSequencial);

		// Executing is allowed for "Nested" jobs
		const Bool canAddPrerequisite = currentState == EJobState::Inactive || currentState == EJobState::Pending || currentState == EJobState::Executing;

		SPT_CHECK_MSG(canAddPrerequisite, "AddPrerequisite called in invalid state ({}) on job \"{}\". Prerequisite - \"{}\"",
					  static_cast<Uint32>(currentState), GetName(), job->GetName());

		const lib::LockGuard prerequisitesLockGuard(m_prerequisitesLock);

		m_remainingPrerequisitesNum.fetch_add(1, impl::MemoryOrderAcquireRelease);
		m_prerequisites.emplace_back(job);
		job->AddConsequent(this);

		// Sanity check if we're still in valid state
		SPT_CHECK(m_jobState.load(impl::MemoryOrderSequencial) == currentState);
	}

	void AddConsequent(lib::MTHandle<JobInstance> next)
	{
		SPT_CHECK(next.IsValid());

		m_consequentsLock.lock();

		EJobState currentJobState = m_jobState.load(impl::MemoryOrderSequencial);

		if (currentJobState == EJobState::Finishing)
		{
			m_consequentsLock.unlock();
			while (currentJobState != EJobState::Finished)
			{
				platf::Platform::SwitchToThread();
				currentJobState = m_jobState.load(impl::MemoryOrderSequencial);
			}
			m_consequentsLock.lock();
		}

		if(currentJobState != EJobState::Finished)
		{
			m_consequents.emplace_back(std::move(next));
		}
		else
		{
			next->PostPrerequisiteExecuted();
		}

		m_consequentsLock.unlock();
	}

	void OnConstructed()
	{
		if (!IsEventJob())
		{
			Activate();
		}
	}

	void TryExecutePrerequisites_Lockless(const lib::DynamicArray<lib::MTHandle<JobInstance>>& prerequisitesToExecute) const
	{
		for (const lib::MTHandle<JobInstance>& prerequisite : prerequisitesToExecute)
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
		lib::DynamicArray<lib::MTHandle<JobInstance>> prerequisitesCopy;
		{
			const lib::LockGuard lockGuard(m_prerequisitesLock);
			prerequisitesCopy = m_prerequisites;
		}

		TryExecutePrerequisites_Lockless(prerequisitesCopy);
	}

	void Execute()
	{
		const JobExecutionScope executionScope(*this);

		// Nested jobs should tread this job as "Inline"
		lib::RemoveFlag(m_flags, EJobFlags::Inline);

		{
			const lib::LockGuard lockGuard(m_prerequisitesLock);
			m_prerequisites.clear();
		}

		m_remainingPrerequisitesNum.fetch_add(1, impl::MemoryOrderRelease);
		DoExecute();
		m_remainingPrerequisitesNum.fetch_add(-1, impl::MemoryOrderRelease);

		if (!CanFinishExecution())
		{
			TryExecutePrerequisites_Locked();
		}

		m_jobState.store(EJobState::Executed, impl::MemoryOrderSequencial);
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
		if (!CanFinishExecution())
		{
			return false;
		}

		EJobState expected = EJobState::Executed;
		const Bool finishThisThread = m_jobState.compare_exchange_strong(expected, EJobState::Finishing, impl::MemoryOrderSequencial);

		SPT_CHECK(expected != EJobState::Inactive && expected != EJobState::Pending && expected != EJobState::Scheduled);

		if (finishThisThread)
		{
			{
				const lib::LockGuard lockGuard(m_prerequisitesLock);
				m_prerequisites.clear();
			}

			lib::DynamicArray<lib::MTHandle<JobInstance>> consequentsCopy;
			{
				const lib::LockGuard lockGuard(m_consequentsLock);

				consequentsCopy = std::move(m_consequents);
			}

			SPT_CHECK(m_consequents.empty() && m_prerequisites.empty());

			m_jobState.store(EJobState::Finished, impl::MemoryOrderSequencial);

			// During next for loop we this job may be destroyed during call to PostPrerequisiteExecuted if consequent had only reference to this job
			// This may happen if this job is nested
			// Because of this we don't want to use any members from now on, so we need to move consequents to local array

			// First we loop over all jobs to decrement their prerequisites count and add to scheduler (not inline jobs)
			for (lib::MTHandle<JobInstance>& consequent : consequentsCopy)
			{
				JobInstance* consequentPtr = consequent.Get();
				// make sure we release all references before finishing local job
				if (consequentPtr->IsLocal() && !consequentPtr->IsInline())
				{
					SPT_CHECK(consequentPtr->GetRefCount() > 0u);
					consequent.Reset();
				}
				consequentPtr->PostPrerequisiteExecuted();
			}

			// Then we loop over all jobs to execute inline consequents
			for (lib::MTHandle<JobInstance>& consequent : consequentsCopy)
			{
				JobInstance* consequentPtr = consequent.Get();
				if (consequentPtr && consequentPtr->IsInline())
				{
					// make sure we release all references before finishing local job
					if (consequentPtr->IsLocal())
					{
						SPT_CHECK(consequentPtr->GetRefCount() > 0u);
						consequent.Reset();
					}

					consequentPtr->TryExecute();
				}
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
				// Immediate job will be executed without scheduling
				if (!IsInline())
				{
					TrySchedule();
				}
			}
			else if(currentState == EJobState::Executed)
			{
				// Finished nested jobs
				TryFinish();
			}
		}
	}

	void InitInternal(const JobDefinitionInternal& def)
	{
		m_priority = def.priority;
		m_flags    = def.flags;

		if (def.executeBeforeEvent.IsValid())
		{
			def.executeBeforeEvent->AddPrerequisite(this);
		}

	}

	void Activate()
	{
		EJobState previous = EJobState::Inactive;
		m_jobState.compare_exchange_strong(OUT previous, EJobState::Pending, impl::MemoryOrderSequencial);
		SPT_CHECK(previous == EJobState::Inactive);

		if (m_remainingPrerequisitesNum.load() == 0)
		{
			if(IsInline())
			{
				TryExecute();
			}
			else
			{
				TrySchedule();
			}
		}
	}

	void TrySchedule()
	{
		EJobState previous = EJobState::Pending;
		const Bool scheduleOnThisThread = m_jobState.compare_exchange_strong(previous, EJobState::Scheduled, impl::MemoryOrderSequencial);
		if(scheduleOnThisThread)
		{
			Scheduler::ScheduleJob(this);
		}
	}

	lib::DynamicArray<lib::MTHandle<JobInstance>>	m_prerequisites;
	std::atomic<Int32>								m_remainingPrerequisitesNum;
	mutable lib::Lock								m_prerequisitesLock;

	std::atomic<EJobState> m_jobState;

	lib::DynamicArray<lib::MTHandle<JobInstance>>	m_consequents;
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
	static auto Build(const char* name, TCallable&& callable, TPrerequisitesRange&& prerequisites, const JobDefinitionInternal& def)
	{
		SPT_PROFILER_FUNCTION();

		lib::MTHandle<JobInstance> job;
		{
			job = new JobInstance(name);

			SPT_CHECK(job.IsValid());
		}
		job->Init(std::forward<TCallable>(callable), std::forward<TPrerequisitesRange>(prerequisites), def);
		return job;
	}

	template<typename TCallable>
	static auto Build(const char* name, TCallable&& callable, const JobDefinitionInternal& def)
	{
		SPT_PROFILER_FUNCTION();

		lib::MTHandle<JobInstance> job;
		{
			job = new JobInstance(name);

			SPT_CHECK(job.IsValid());
		}
		job->Init(std::forward<TCallable>(callable), def);
		return job;
	}

	template<typename TCallable>
	static auto InitLocal(JobInstance& job, TCallable&& callable, const JobDefinitionInternal& def, Byte* resultPtr = nullptr)
	{
		SPT_PROFILER_FUNCTION();

		job.AddRef();

		using CallableResult = impl::JobInvokeResult<TCallable>::Type;

		auto callableWrapper = [&callable, resultPtr]()
		{
			if constexpr (std::is_same_v<CallableResult, void>)
			{
				callable();
			}
			else
			{
				lib::TypeStorage<CallableResult>* resultStorage = resultPtr;
				resultStorage.Construct(callable());
			}
		};

		job.Init(callableWrapper, def);
	}
};


class Job
{
public:

	Job()
		: m_instance(nullptr)
	{ }

	explicit Job(lib::MTHandle<JobInstance> inInstance)
		: m_instance(std::move(inInstance))
	{ }

	void Wait() const
	{
		m_instance->Wait();
	}

	const lib::MTHandle<JobInstance>& GetJobInstance() const
	{
		return m_instance;
	}

	Bool IsValid() const
	{
		return m_instance.IsValid();
	}

	template<typename TCallable>
	Job Then(const char* name, TCallable&& callable)
	{
		js::JobDefinitionInternal def;
		def.priority = m_instance->GetPriority();
		def.flags    = m_instance->GetFlags();
		lib::MTHandle<JobInstance> instance =  JobInstanceBuilder::Build(name, callable, Prerequisites(*this), def);
		return Job(std::move(instance));
	}

protected:

	void SetJobInstance(lib::MTHandle<JobInstance> inInstance)
	{
		SPT_CHECK(!m_instance.IsValid());
		m_instance = std::move(inInstance);
	}

	void ClearJobHandle()
	{
		m_instance.Reset();
	}

private:

	lib::MTHandle<JobInstance> m_instance;
};


template<typename TResultType>
class JobWithResult : public Job
{
public:

	JobWithResult() = default;

	explicit JobWithResult(lib::MTHandle<JobInstance> inInstance)
		: Job(std::move(inInstance))
	{ }

	JobWithResult(JobWithResult& rhs) = default;
	JobWithResult(JobWithResult&& rhs) = default;

	JobWithResult& operator=(JobWithResult& rhs) = default;
	JobWithResult& operator=(JobWithResult&& rhs) = default;

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


template<typename TJobType>
class LocalJob : public TJobType
{
protected:

	using Super = TJobType;

public:

	explicit LocalJob(lib::MTHandle<JobInstance> inInstance)
		: Super(inInstance)
	{
		inInstance->AddRef();
	}

	~LocalJob()
	{
		JobInstance& job = *TJobType::GetJobInstance();
		job->Wait();
		TJobType::ClearJobHandle();

		const Bool canBeDestroed = job.Release();
		SPT_CHECK(canBeDestroed);
	}
};


class Event : public Job
{
public:

	Event() = default;

	explicit Event(lib::MTHandle<JobInstance> inInstance)
	{
		SPT_CHECK(inInstance.IsValid());
		SPT_CHECK(inInstance->IsEventJob());

		SetJobInstance(std::move(inInstance));
	}

	void Signal() const
	{
		SPT_CHECK(GetJobInstance().IsValid());
		GetJobInstance()->Signal();
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

	static lib::MTHandle<JobInstance> GetInstance(const Job& job)
	{
		return job.GetJobInstance();
	}
};

template<typename TResultType>
struct JobInstanceGetter<JobWithResult<TResultType>> : public JobInstanceGetter<Job> {};

template<typename TUnderlyingJobType>
struct JobInstanceGetter<LocalJob<TUnderlyingJobType>> : public JobInstanceGetter<TUnderlyingJobType> {};

template<>
struct JobInstanceGetter<Event> : public JobInstanceGetter<Job> {};

template<>
struct JobInstanceGetter<lib::MTHandle<JobInstance>>
{
public:

	static lib::MTHandle<JobInstance> GetInstance(const lib::MTHandle<JobInstance>& job)
	{
		return job;
	}
};

template<>
struct JobInstanceGetter<JobInstance*>
{
public:

	static lib::MTHandle<JobInstance> GetInstance(JobInstance* job)
	{
		return job;
	}
};

} // impl


class JobBuilder
{
public:

	template<typename TCallable, Bool isLocal = false>
	static auto Build(lib::MTHandle<JobInstance> job)
	{
		return CreateJobWrapper<TCallable, isLocal>(std::move(job));
	}

	static Event BuildEvent(lib::MTHandle<JobInstance> job)
	{
		return Event(job);
	}

private:

	template<typename TCallable, Bool isLocal>
	static auto CreateJobWrapper(lib::MTHandle<JobInstance> jobInstance)
	{
		using CallableResult = impl::JobInvokeResult<TCallable>::Type;
		using UnderlyingJobType = std::conditional_t<std::is_same_v<CallableResult, void>, Job, JobWithResult<CallableResult>>;
		using JobType = std::conditional_t<isLocal, LocalJob<UnderlyingJobType>, UnderlyingJobType>;

		return JobType(std::move(jobInstance));
	}
};

} // spt::js