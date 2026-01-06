#pragma once

#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIFwd.h"


namespace spt::rdr
{
class CommandRecorder;
class BLASBuilder;
} // spt::rdr


namespace spt::gfx
{

class GPUDeferredUploadRequest
{
public:

	virtual ~GPUDeferredUploadRequest() = default;

	virtual void PrepareForUpload(rdr::CommandRecorder& cmdRecorder, rhi::RHIDependency& preUploadDependency) {};
	virtual void EnqueueUploads() = 0;
	virtual void FinishStreaming(rdr::CommandRecorder& cmdRecorder, rhi::RHIDependency& postUploadDependency) {};
};


class GPUDeferredBLASBuildRequest
{
public:

	virtual ~GPUDeferredBLASBuildRequest() = default;

	virtual void BuildBLASes(rdr::BLASBuilder& builder) = 0;
};

} // spt::gfx
