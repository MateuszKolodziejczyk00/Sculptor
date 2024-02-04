#pragma once

#include "SculptorLibMacros.h"


namespace spt::lib
{

class SCULPTOR_LIB_API Waitable
{
public:

	virtual ~Waitable() = default;

	virtual void Wait() = 0;
};

} // spt::lib