#pragma once

#include "GraphicsMacros.h"
#include "SculptorCoreTypes.h"
#include "Delegates/Delegate.h"


namespace spt::gfx
{

class GRAPHICS_API Readback
{
public:

	using Delegate = lib::Delegate<void()>;

	static void ScheduleReadback(Delegate delegate);

private:
	
	Readback() = default;
};

} // spt::gfx
