#pragma once

#include "GraphicsMacros.h"
#include "SculptorCoreTypes.h"
#include "Plugins/Plugin.h"
#include "Delegates/MulticastDelegate.h"


namespace spt::rdr
{
class RenderContext;
} // spt::rdr


namespace spt::gfx
{

class GPUDeferredUploadRequest;
class GPUDeferredBLASBuildRequest;


using PostDeferredUploadsMulticastDelegate = lib::ThreadSafeMulticastDelegate<void()>;


class GRAPHICS_API GPUDeferredCommandsQueue : public engn::Plugin
{
	SPT_GENERATE_PLUGIN(GPUDeferredCommandsQueue);

	using Super = Plugin;

public:

	void RequestUpload(lib::UniquePtr<GPUDeferredUploadRequest> request);
	void RequestBLASBuild(lib::UniquePtr<GPUDeferredBLASBuildRequest> request);

	void ForceFlushCommands();

	void AddPostDeferredUploadsDelegate(PostDeferredUploadsMulticastDelegate::Delegate delegate)
	{
		m_postDeferredUploadsDelegate.Add(std::move(delegate));
	}

protected:

	// Begin Plugin overrides
	virtual void OnPostEngineInit() override;
	virtual void OnBeginFrame(engn::FrameContext& frameContext) override;
	// End Plugin overrides

private:

	void ExecuteCommands();

	void ExecutePreUploadBarriers(const lib::SharedPtr<rdr::RenderContext>& renderContext);
	void ExecutePostUploadBarriers(const lib::SharedPtr<rdr::RenderContext>& renderContext);

	lib::Spinlock m_requestsLock;


	lib::DynamicArray<lib::UniquePtr<GPUDeferredUploadRequest>> m_uploadRequests;
	lib::DynamicArray<lib::UniquePtr<GPUDeferredBLASBuildRequest>> m_blasBuildRequests;

	PostDeferredUploadsMulticastDelegate m_postDeferredUploadsDelegate;
};

} // spt::gfx
