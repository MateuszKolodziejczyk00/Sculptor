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


using PostDeferredUploadsMulticastDelegate = lib::ThreadSafeMulticastDelegate<void()>;


class GRAPHICS_API GPUDeferredUploadsQueue : public engn::Plugin
{
	SPT_GENERATE_PLUGIN(GPUDeferredUploadsQueue);

	using Super = Plugin;

public:

	void RequestUpload(lib::UniquePtr<GPUDeferredUploadRequest> request);

	void ForceFlushUploads();

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

	void ExecuteUploads();

	void ExecutePreUploadBarriers(const lib::SharedPtr<rdr::RenderContext>& renderContext);
	void ExecutePostUploadBarriers(const lib::SharedPtr<rdr::RenderContext>& renderContext);


	lib::DynamicArray<lib::UniquePtr<GPUDeferredUploadRequest>> m_requests;

	PostDeferredUploadsMulticastDelegate m_postDeferredUploadsDelegate;
};

} // spt::gfx
