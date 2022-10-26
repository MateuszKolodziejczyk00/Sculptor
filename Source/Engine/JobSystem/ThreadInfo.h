#pragma once

#include "SculptorCoreTypes.h"


namespace spt::js
{

struct ThreadInfoDefinition
{
	ThreadInfoDefinition()
		: isWorker(false)
	{ }

	Bool isWorker;
};

class ThreadInfoTls
{
public:

	static void Init(const ThreadInfoDefinition& info)
	{
		isWorker = info.isWorker;
	}

	static Bool IsWorker()
	{
		return isWorker;
	}

private:

	inline static thread_local Bool isWorker = false;
};

} // spt::js