#pragma once

#include "SculptorCoreTypes.h"
#include "Commands/RHIRenderingDefinition.h"


namespace spt::rhi
{

enum class ECommandBufferType
{
	Primary,
	Secondary
};


enum class EDeviceCommandQueueType
{
	Graphics,
	AsyncCompute,
	Transfer,
	NUM
};


namespace ECommandBufferComplexityClass
{

enum Type : Uint32
{
	Low,
	Default,
	High,

	NUM
};

}


struct CommandBufferDefinition
{
	CommandBufferDefinition() = default;

	CommandBufferDefinition(EDeviceCommandQueueType inQueueType, ECommandBufferType inCmdBufferType, ECommandBufferComplexityClass::Type inComplexityClass = ECommandBufferComplexityClass::Default)
		: queueType(inQueueType)
		, cmdBufferType(inCmdBufferType)
		, complexityClass(inComplexityClass)
	{ }

	EDeviceCommandQueueType					queueType       = EDeviceCommandQueueType::Graphics;
	ECommandBufferType						cmdBufferType   = ECommandBufferType::Primary;
	ECommandBufferComplexityClass::Type		complexityClass = ECommandBufferComplexityClass::Default;
};


enum class ECommandBufferBeginFlags : Flags32
{
	None              = 0,
	OneTimeSubmit     = BIT(0),
	ContinueRendering = BIT(1),

	Default = OneTimeSubmit
};


struct CommandBufferUsageDefinition
{
	CommandBufferUsageDefinition()
		: beginFlags(ECommandBufferBeginFlags::Default)
	{ }

	CommandBufferUsageDefinition(ECommandBufferBeginFlags inBeginFlags)
		: beginFlags(inBeginFlags)
	{ }

	ECommandBufferBeginFlags                      beginFlags;
	std::optional<RenderingInheritanceDefinition> renderingInheritance;
};

} // spt::rhi


namespace spt::lib
{

template<>
struct Hasher<rhi::CommandBufferDefinition>
{
    size_t operator()(const rhi::CommandBufferDefinition& cmdBufferDef) const
    {
		return HashCombine(cmdBufferDef.queueType,
						   cmdBufferDef.cmdBufferType,
						   cmdBufferDef.complexityClass);
    }
};


} // spt::lib