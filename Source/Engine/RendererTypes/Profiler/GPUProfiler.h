#pragma once

#include "RendererTypesMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIProfilerImpl.h"
#include "GPUProfiler.h"


namespace spt::rdr
{

class Window;
class CommandBuffer;


class RENDERER_TYPES_API GPUProfiler
{
public:

	using CmdBufferContext = rhi::RHIProfiler::CmdBufferContext;

	static void				Initialize();

	static void				FlipFrame(const lib::SharedPtr<Window>& window);

	static CmdBufferContext	GetCmdBufferContext(const lib::SharedPtr<CommandBuffer>& cmdBuffer);
};

} // spt::rdr


#if PROFILE_GPU

#if WITH_OPTICK

#if VULKAN_RHI

#define SPT_GPU_PROFILER_CONTEXT_CAST(Context) reinterpret_cast<VkCommandBuffer>(Context)

#else

#error Only VulkanRHI is supported

#endif // VULKAN_RHI

#define SPT_GPU_PROFILER_CONTEXT(cmdBuffer)	OPTICK_GPU_CONTEXT(SPT_GPU_PROFILER_CONTEXT_CAST(spt::rdr::GPUProfiler::GetCmdBufferContext(cmdBuffer)))
#define SPT_GPU_PROFILER_EVENT(Name)		OPTICK_GPU_EVENT(Name)

#endif // WITH_OPTICK

#else

#define SPT_GPU_PROFILER_CONTEXT(cmdBuffer)
#define SPT_GPU_PROFILER_EVENT(Name)

#endif // PROFILE_GPI
