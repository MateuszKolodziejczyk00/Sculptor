#pragma once

#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIFwd.h"


namespace spt::rdr
{
class CommandRecorder;
} // spt::rdr


namespace spt::gfx
{

class GPUDeferredUploadRequest
{
public:

	virtual ~GPUDeferredUploadRequest() = default;

	virtual void PrepareForUpload(rdr::CommandRecorder& cmdRecorder, rhi::RHIDependency& preUploadDependency) = 0;
	virtual void EnqueueUploads() = 0;
	virtual void FinishStreaming(rdr::CommandRecorder& cmdRecorder, rhi::RHIDependency& postUploadDependency) = 0;
};

} // spt::gfx
