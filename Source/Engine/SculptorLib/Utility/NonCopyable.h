#pragma once

#include "SculptorLibMacros.h"


namespace spt::lib
{

class SCULPTOR_LIB_API NonCopyable
{
protected:

	NonCopyable() = default;
	~NonCopyable() = default;

private:

	NonCopyable(const NonCopyable&) = delete;
	NonCopyable& operator=(const NonCopyable&) = delete;
};

} // spt::lib