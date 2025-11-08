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
	EPolygonMode       polygonMode       = EPolygonMode::Fill;
	ECullMode          cullMode          = ECullMode::Back;
	ERasterizationType rasterizationType = ERasterizationType::Default;
};


struct MultisamplingDefinition
{
	Uint32 samplesNum = 1;
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
	EFragmentFormat             format         = EFragmentFormat::None;
	ERenderTargetBlendType      colorBlendType = ERenderTargetBlendType::Copy;
	ERenderTargetBlendType      alphaBlendType = ERenderTargetBlendType::Copy;
	ERenderTargetComponentFlags colorWriteMask = ERenderTargetComponentFlags::All;
};


struct DepthRenderTargetDefinition
{
	EFragmentFormat format           = EFragmentFormat::None;
	ECompareOp      depthCompareOp   = ECompareOp::Greater;
	Bool            enableDepthWrite = true;
};


struct StencilRenderTargetDefinition
{
	EFragmentFormat format = EFragmentFormat::None;
};


struct PipelineRenderTargetsDefinition
{
	lib::DynamicArray<ColorRenderTargetDefinition> colorRTsDefinition;
	DepthRenderTargetDefinition                    depthRTDefinition;
	StencilRenderTargetDefinition                  stencilRTDefinition;
};


struct GraphicsPipelineDefinition
{
	EPrimitiveTopology              primitiveTopology = EPrimitiveTopology::TriangleList;
	PipelineRasterizationDefinition rasterizationDefinition;
	MultisamplingDefinition         multisamplingDefinition;
	PipelineRenderTargetsDefinition renderTargetsDefinition;
};


struct RayTracingHitGroupDefinition
{
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
	Uint32 maxRayRecursionDepth = 1u;
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
