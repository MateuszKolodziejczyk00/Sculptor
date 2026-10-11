#pragma once

#include "SculptorCoreTypes.h"


namespace spt::tools
{

class ScopedEngine
{
public:

	ScopedEngine();
	~ScopedEngine();

	ScopedEngine(const ScopedEngine&) = delete;
	ScopedEngine& operator=(const ScopedEngine&) = delete;
};

} // spt::tools
