#pragma once

#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIFwd.h"


namespace spt::rdr
{
class CommandRecorder;
} // spt::rdr


namespace spt::as
{

class StreamableTextureInterface
{
public:

	virtual void PrepareForUpload(rdr::CommandRecorder& cmdRecorder, rhi::RHIDependency& preUploadDependency) = 0;
	virtual void ScheduleUploads() = 0;
	virtual void FinishStreaming(rdr::CommandRecorder& cmdRecorder, rhi::RHIDependency& postUploadDependency) = 0;
};

} // spt::as