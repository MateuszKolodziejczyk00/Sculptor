#pragma once

#include "SculptorCoreTypes.h"
#include "RHI/RHICore/RHIPipelineTypes.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rg::capture
{


class RGCaptureSourceContext;


struct RGCaptureSourceInfo
{
	lib::WeakPtr<RGCaptureSourceContext> sourceContext;
};

enum class ECapturedResourceFlags
{
	None = 0,
};


struct RGResourceCapture
{
	ECapturedResourceFlags flags;
	lib::HashedString      passName;
};


struct RGTextureCapture : public RGResourceCapture
{
	lib::SharedPtr<rdr::TextureView> textureView;
};


struct RGCapturedNullProperties
{

};

struct RGCapturedComputeProperties
{
	rhi::PipelineStatistics pipelineStatistics;
};


using RGNodeExecutionStatistics = std::variant<RGCapturedNullProperties, RGCapturedComputeProperties>;


struct RGNodeCapture
{
	lib::HashedString name;

	lib::DynamicArray<RGTextureCapture> outputTextures;

	RGNodeExecutionStatistics properties;
};


struct RGCapture
{
	lib::DynamicArray<RGNodeCapture> nodes;

	RGCaptureSourceInfo captureSource;
};

} // spt::rg::capture
