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
	PointList,
	LineList,
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
	None
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


enum class ERenderTargetBlendType
{
	Copy,
	Add
};


enum class ERenderTargetComponentFlags
{
	R = BIT(0),
	G = BIT(1),
	B = BIT(2),
	A = BIT(3),

	None	= 0,
	All		= R | G | B | A
};


struct ColorRenderTargetDefinition
{
	explicit ColorRenderTargetDefinition(EFragmentFormat inFormat = EFragmentFormat::None, ERenderTargetBlendType inBlendType = ERenderTargetBlendType::Copy, ERenderTargetComponentFlags inWriteMask = ERenderTargetComponentFlags::All)
		: format(inFormat)
		, colorBlendType(inBlendType)
		, alphaBlendType(inBlendType)
		, colorWriteMask(inWriteMask)
	{ }

	EFragmentFormat				format;
	ERenderTargetBlendType		colorBlendType;
	ERenderTargetBlendType		alphaBlendType;
	ERenderTargetComponentFlags	colorWriteMask;
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
	explicit StencilRenderTargetDefinition(EFragmentFormat inFormat = EFragmentFormat::None)
		: format(inFormat)
	{ }
	
	EFragmentFormat		format;
};


struct PipelineRenderTargetsDefinition
{
	PipelineRenderTargetsDefinition()
	{ }

	lib::DynamicArray<ColorRenderTargetDefinition>	colorRTsDefinition;
	DepthRenderTargetDefinition						depthRTDefinition;
	StencilRenderTargetDefinition					stencilRTDefinition;
};


struct GraphicsPipelineDefinition
{
	GraphicsPipelineDefinition()
		: primitiveTopology(EPrimitiveTopology::TriangleList)
	{ }

	EPrimitiveTopology					primitiveTopology;
	PipelineRasterizationDefinition		rasterizationDefinition;
	MultisamplingDefinition				multisamplingDefinition;
	PipelineRenderTargetsDefinition		renderTargetsDefinition;
};

} //spt::rhi
