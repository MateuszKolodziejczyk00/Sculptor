#pragma once

#include "SculptorCoreTypes.h"


namespace spt::js
{

class JobInstance;


namespace EJobPriority
{

enum Type
{
	High,
	Medium,
	Low,

	Num,

	Default = Medium
};

} // EJobPriority


enum class EJobFlags
{
	None             = 0,

	// Job is created on stack
	Local            = BIT(1),
	// Launches job on calling thread is it doesn't have prerequisites. Otherwise it's executed immediately after last prerequisite is finished
	Inline           = BIT(2),
	// Job is always pushed to global queue
	ForceGlobalQueue = BIT(3),
	// Job must be "signaled" before it can be executed
	EventJob         = BIT(4),

	Default = None
};

static constexpr SizeType g_maxWorkerThreadsNum = 32;

struct JobDefinitionInternal
{
	JobDefinitionInternal() = default;

	EJobPriority::Type         priority = EJobPriority::Default;
	EJobFlags                  flags = EJobFlags::Default;
	lib::MTHandle<JobInstance> executeBeforeEvent;
};

} // spt::js