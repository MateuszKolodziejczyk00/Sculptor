#pragma once

#include "SculptorCoreTypes.h"


namespace spt::js
{

enum class EJobPriority
{
	Low,
	Medium,
	High,

	Num,

	Default = Medium
};


enum class EJobFlags
{
	None					= 0,

	Default = None
};

} // spt::js