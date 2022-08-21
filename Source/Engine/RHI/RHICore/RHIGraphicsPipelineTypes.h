#pragma once

#include "SculptorCoreTypes.h"
#include "RHIPipelineTypes.h"
#include "RHITextureTypes.h"
#include "RHIBridge/RHIShaderModuleImpl.h"


namespace spt::rhi
{

struct PipelineShaderStagesDefinition
{
	PipelineShaderStagesDefinition()
	{ }

	lib::DynamicArray<RHIShaderModule> shaderModules;
};


enum class EPrimitiveTopology
{
	Point,
	Line,
	LineStrip,
	TriangleList,
	TriangleStrip,
	TriangleFan
};


enum class EPolygonMode
{
	Fill,
	Wireframe
};


enum class ECullMode
{
	Back,
	Front,
	BackAndFront,
	None,
};


struct PipelineRasterizationDefinition
{
	explicit PipelineRasterizationDefinition(EPolygonMode inPolygonMode = EPolygonMode::Fill, ECullMode inCullMode = ECullMode::Back)
		: polygonMode(inPolygonMode)
		, cullMode(inCullMode)
	{ }

	EPolygonMode		polygonMode;
	ECullMode			cullMode;
};


struct MultisamplingDefinition
{
	explicit MultisamplingDefinition(Uint32 inSamplesNum = 1)
		: samplesNum(inSamplesNum)
	{ }

	Uint32	samplesNum;
};


enum class EDepthCompareOperation
{
	None,
	Less,
	LessOrEqual,
	Greater,
	GreaterOrEqual,
	Equal,
	NotEqual,
	Always,
	Never
};


enum class EColorBlendType
{
	Copy,
	Add
};


struct ColorRenderTargetDefinition
{
	explicit ColorRenderTargetDefinition(EFragmentFormat inFormat = EFragmentFormat::None, EColorBlendType inBlendType = EColorBlendType::Copy)
		: format(inFormat)
		, blendMode(inBlendType)
	{ }

	EFragmentFormat			format;
	EColorBlendType			blendMode;
};


struct DepthRenderTargetDefinition
{
	explicit DepthRenderTargetDefinition(EFragmentFormat inFormat = EFragmentFormat::None, EDepthCompareOperation inDepthCompareOp = EDepthCompareOperation::Less, Bool inEnableDepthWrite = true)
		: format(inFormat)
		, depthCompareOp(inDepthCompareOp)
		, enableDepthWrite(inEnableDepthWrite)
	{ }

	EFragmentFormat			format;
	EDepthCompareOperation	depthCompareOp;
	Bool					enableDepthWrite;
};


struct StencilRenderTargetDefinition
{
	explicit StencilRenderTargetDefinition(EFragmentFormat inFormat)
		: format(inFormat)
	{ }
	
	EFragmentFormat		format;
};


struct PipelineRenderTargetsDefinition
{
	PipelineRenderTargetsDefinition()
	{ }

	lib::DynamicArray<ColorRenderTargetDefinition>	colorRTsFormat;
	DepthRenderTargetDefinition						depthRTFormat;
	StencilRenderTargetDefinition					stencilRTFormat;
};

struct GraphicsPipelineDefinition
{
	GraphicsPipelineDefinition()
		: primitiveTopology(EPrimitiveTopology::TriangleList)
	{ }

	PipelineShaderStagesDefinition		shaderStagesDefinition;
	EPrimitiveTopology					primitiveTopology;
	PipelineRasterizationDefinition		RasterizationDefinition;
	MultisamplingDefinition				MultisamplingDefinition;
	PipelineRenderTargetsDefinition		renderTargetsDefinition;
};

} //spt::rhi
