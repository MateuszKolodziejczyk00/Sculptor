#pragma once

#include "SculptorCoreTypes.h"
#include "RHICommonTypes.h"
#include "RHIPipelineTypes.h"
#include "RHITextureTypes.h"
#include "RHIBridge/RHIShaderModuleImpl.h"


namespace spt::rhi
{

struct GraphicsPipelineShadersDefinition
{
	RHIShaderModule vertexShader;
	RHIShaderModule taskShader;
	RHIShaderModule meshShader;
	RHIShaderModule fragmentShader;
};


enum class EPrimitiveTopology : Uint8
{
	PointList,
	LineList,
	LineStrip,
	TriangleList,
	TriangleStrip,
	TriangleFan
};


enum class EPolygonMode : Uint8
{
	Fill,
	Wireframe
};


enum class ECullMode : Uint8
{
	Back,
	Front,
	BackAndFront,
	None
};


enum class ERasterizationType : Uint8
{
	Default,
	ConservativeOverestimate,
	ConservativeUnderestimate
};


struct PipelineRasterizationDefinition
{
	explicit PipelineRasterizationDefinition(EPolygonMode inPolygonMode = EPolygonMode::Fill, ECullMode inCullMode = ECullMode::Back, ERasterizationType inRasterizationType = ERasterizationType::Default)
		: polygonMode(inPolygonMode)
		, cullMode(inCullMode)
		, rasterizationType(inRasterizationType)
	{ }

	EPolygonMode		polygonMode;
	ECullMode			cullMode;
	ERasterizationType	rasterizationType;
};


struct MultisamplingDefinition
{
	explicit MultisamplingDefinition(Uint32 inSamplesNum = 1)
		: samplesNum(inSamplesNum)
	{ }

	Uint32 samplesNum;
};


enum class ERenderTargetBlendType : Uint8
{
	Disabled,
	Copy,
	Add
};


enum class ERenderTargetComponentFlags : Uint8
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

	EFragmentFormat             format;
	ERenderTargetBlendType      colorBlendType;
	ERenderTargetBlendType      alphaBlendType;
	ERenderTargetComponentFlags colorWriteMask;
};


struct DepthRenderTargetDefinition
{
	explicit DepthRenderTargetDefinition(EFragmentFormat inFormat = EFragmentFormat::None, ECompareOp inDepthCompareOp = ECompareOp::Greater, Bool inEnableDepthWrite = true)
		: format(inFormat)
		, depthCompareOp(inDepthCompareOp)
		, enableDepthWrite(inEnableDepthWrite)
	{ }

	EFragmentFormat		format;
	ECompareOp			depthCompareOp;
	Bool				enableDepthWrite;
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


struct RayTracingHitGroupDefinition
{
	RayTracingHitGroupDefinition()
	{ }

	SizeType GetValidShadersNum() const
	{
		return (closestHitModule.IsValid() ? 1 : 0)
			 + (anyHitModule.IsValid() ? 1 : 0)
			 + (intersectionModule.IsValid() ? 1 : 0);
	}

	RHIShaderModule closestHitModule;
	RHIShaderModule anyHitModule;
	RHIShaderModule intersectionModule;
};


struct RayTracingShadersDefinition
{
	RayTracingShadersDefinition()
	{ }

	SizeType GetHitShadersNum() const
	{
		return std::accumulate(std::cbegin(hitGroups), std::cend(hitGroups),
							   SizeType(0),
							   [](SizeType currentValue, const RayTracingHitGroupDefinition& hitGroup)
							   {
								   return currentValue + hitGroup.GetValidShadersNum();
							   });
	}

	RHIShaderModule rayGenerationModule;
	lib::DynamicArray<RayTracingHitGroupDefinition> hitGroups;
	lib::DynamicArray<RHIShaderModule> missModules;
};


struct RayTracingPipelineDefinition
{
	RayTracingPipelineDefinition()
		: maxRayRecursionDepth(1)
	{ }

	Uint32 maxRayRecursionDepth;
};

} //spt::rhi


namespace spt::lib
{

template<>
struct Hasher<rhi::PipelineRasterizationDefinition>
{
	constexpr size_t operator()(const rhi::PipelineRasterizationDefinition& rasterizationDef) const
	{
		return HashCombine(rasterizationDef.cullMode,
									 rasterizationDef.polygonMode);;
	}
};


template<>
struct Hasher<rhi::MultisamplingDefinition>
{
	constexpr size_t operator()(const rhi::MultisamplingDefinition& multisamplingDef) const
	{
		return GetHash(multisamplingDef.samplesNum);
	}
};


template<>
struct Hasher<rhi::ColorRenderTargetDefinition>
{
	constexpr size_t operator()(const rhi::ColorRenderTargetDefinition& colorRTDef) const
	{
		return HashCombine(colorRTDef.format,
						   colorRTDef.colorWriteMask,
						   colorRTDef.alphaBlendType,
						   colorRTDef.colorBlendType);
	}
};


template<>
struct Hasher<rhi::DepthRenderTargetDefinition>
{
	constexpr size_t operator()(const rhi::DepthRenderTargetDefinition& depthRTDef) const
	{
		return HashCombine(depthRTDef.format,
						   depthRTDef.depthCompareOp,
						   depthRTDef.enableDepthWrite);
	}
};


template<>
struct Hasher<rhi::StencilRenderTargetDefinition>
{
	constexpr size_t operator()(const rhi::StencilRenderTargetDefinition& stencilRTDef) const
	{
		return GetHash(stencilRTDef.format);
	}
};


template<>
struct Hasher<rhi::PipelineRenderTargetsDefinition>
{
	constexpr size_t operator()(const rhi::PipelineRenderTargetsDefinition& renderTargetsDef) const
	{
		return HashCombine(HashRange(std::cbegin(renderTargetsDef.colorRTsDefinition),
									 std::cend(renderTargetsDef.colorRTsDefinition)),
						   renderTargetsDef.depthRTDefinition,
						   renderTargetsDef.stencilRTDefinition);
	}
};


template<>
struct Hasher<rhi::GraphicsPipelineDefinition>
{
	constexpr size_t operator()(const rhi::GraphicsPipelineDefinition& pipelineDef) const
	{
		return HashCombine(pipelineDef.primitiveTopology,
						   pipelineDef.rasterizationDefinition,
						   pipelineDef.multisamplingDefinition,
						   pipelineDef.renderTargetsDefinition);
	}
};


template<>
struct Hasher<rhi::RayTracingPipelineDefinition>
{
	constexpr size_t operator()(const rhi::RayTracingPipelineDefinition& pipelineDef) const
	{
		return GetHash(pipelineDef.maxRayRecursionDepth);
	}
};

} // spt::lib
