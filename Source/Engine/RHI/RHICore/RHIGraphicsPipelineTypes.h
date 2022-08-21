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
	explicit PipelineRasterizationDefinition(EPolygonMode polygonMode = EPolygonMode::Fill, ECullMode cullMode = ECullMode::Back)
		: m_polygonMode(polygonMode)
		, m_cullMode(cullMode)
	{ }

	EPolygonMode		m_polygonMode;
	ECullMode			m_cullMode;
};


struct MultisamplingDefinition
{
	explicit MultisamplingDefinition(Uint32 samplesNum = 1)
		: m_samplesNum(samplesNum)
	{ }

	Uint32	m_samplesNum;
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
		: m_format(inFormat)
		, m_blendMode(inBlendType)
	{ }

	EFragmentFormat			m_format;
	EColorBlendType			m_blendMode;
};


struct DepthRenderTargetDefinition
{

	EFragmentFormat			m_format;
	EDepthCompareOperation	m_depthCompareOp;
	Bool					m_enableDepthWrite;
};


struct StencilRenderTargetDefinition
{
	EFragmentFormat		m_format;
};


struct PipelineRenderTargetsDefinition
{
	PipelineRenderTargetsDefinition()
	{ }

	lib::DynamicArray<ColorRenderTargetDefinition>	m_colorRTsFormat;
	DepthRenderTargetDefinition						m_depthRTFormat;
	StencilRenderTargetDefinition					m_stencilRTFormat;
};

struct GraphicsPipelineDefinition
{
	GraphicsPipelineDefinition()
		: m_primitiveTopology(EPrimitiveTopology::TriangleList)
	{ }

	PipelineShaderStagesDefinition		m_shaderStagesDefinition;
	EPrimitiveTopology					m_primitiveTopology;
	PipelineRasterizationDefinition		m_RasterizationDefinition;
	MultisamplingDefinition				m_MultisamplingDefinition;
	PipelineRenderTargetsDefinition		m_renderTargetsDefinition;
};

} //spt::rhi
