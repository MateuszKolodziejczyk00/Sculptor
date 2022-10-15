#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::rdr
{

class CommandRecorder;


class RENDERER_CORE_API DebugRegion
{
public:

	DebugRegion(CommandRecorder& recorder, const lib::HashedString& regionName, const lib::Color& color);
	~DebugRegion();

private:

	CommandRecorder& m_cachedRecorder;
};

} // spt::rdr


#if RENDERER_VALIDATION

#define SPT_GPU_DEBUG_NAME_CONCAT(begin, end) begin ## end
#define SPT_GPU_DEBUG_NAME_CONCAT_EVAL(begin, end) SPT_GPU_DEBUG_NAME_CONCAT(begin, end)

#define SPT_GPU_DEBUG_REGION_NAME SPT_GPU_DEBUG_NAME_CONCAT_EVAL(_region_, __LINE__)

#define SPT_GPU_DEBUG_REGION(recorderRef, name, color) const rdr::DebugRegion SPT_GPU_DEBUG_REGION_NAME (recorderRef, name, color);

#else

#define SPT_GPU_DEBUG_REGION(recorderRef, name, color)

#endif // RENDERER_VALIDATION
