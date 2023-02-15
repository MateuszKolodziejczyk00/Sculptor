#include "Readback.h"
#include "CurrentFrameContext.h"

namespace spt::gfx
{

void Readback::ScheduleReadback(Delegate delegate)
{
	SPT_CHECK(delegate.IsBound());

	rdr::CurrentFrameContext::GetCurrentFrameCleanupDelegate().AddLambda([readbackDel = std::move(delegate)]()
																		 {
																			 SPT_PROFILER_SCOPE("Execute Readback");

																			 readbackDel.ExecuteIfBound();
																		 });
}

} // spt::gfx
