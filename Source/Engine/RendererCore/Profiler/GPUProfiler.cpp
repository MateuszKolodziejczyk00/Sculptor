#include "GPUProfiler.h"
#include "Types/Window.h"
#include "Types/CommandBuffer.h"

namespace spt::rdr
{

void GPUProfiler::Initialize()
{
#if PROFILE_GPU

	rhi::RHIProfiler::Initialize();

#endif // PROFILE_GPU
}

void GPUProfiler::FlipFrame(const lib::SharedPtr<Window>& window)
{
	SPT_CHECK(!!window);

#if PROFILE_GPU

	rhi::RHIProfiler::FlipFrame(window->GetRHI());

#endif // PROFILE_GPU
}

GPUProfiler::CmdBufferContext GPUProfiler::GetCmdBufferContext(const lib::SharedPtr<CommandBuffer>& cmdBuffer)
{
	SPT_CHECK(!!cmdBuffer);

#if PROFILE_GPU

	return rhi::RHIProfiler::GetCommandBufferContext(cmdBuffer->GetRHI());

#else

	return CmdBufferContext{};

#endif // PROFILE_GPU
}

} // spt::rdr
