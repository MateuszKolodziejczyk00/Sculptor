#pragma once

#include "SculptorCoreTypes.h"


namespace spt::js
{

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
	None			= 0,

	Default = None
};

static constexpr SizeType g_maxWorkerThreadsNum = 32;

} // spt::js